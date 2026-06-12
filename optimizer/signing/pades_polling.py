"""Firma PAdES con el miniHSM via la cola de polling (Bloque 8 Forma 2).

async_sign_raw encola el digest en job_queue y espera (async) a que el device lo
recoja en su heartbeat y devuelva la firma (asyncio.sleep, no bloquea el loop).

Soporta: firma invisible/visible, sello (texto + imagen como fondo o a la
izquierda), modo approval/certify (DocMDP) y sellado de tiempo TSA (RFC 3161).
"""
import io
import hashlib
import asyncio

from pyhanko.sign import signers
from pyhanko.sign.fields import SigFieldSpec, MDPPerm, SigSeedSubFilter, enumerate_sig_fields
from pyhanko.sign.signers.pdf_signer import PdfSignatureMetadata
from pyhanko.pdf_utils.incremental_writer import IncrementalPdfFileWriter
from pyhanko_certvalidator.registry import SimpleCertificateStore
from pyhanko.stamp import TextStampStyle
from pyhanko.pdf_utils.images import PdfImage
from pyhanko.pdf_utils.text import TextBoxStyle
from pyhanko.pdf_utils.layout import (
    SimpleBoxLayoutRule, AxisAlignment, Margins, InnerScaling)
from pyhanko.pdf_utils.font.opentype import GlyphAccumulatorFactory
try:
    from pyhanko.sign.timestamps import HTTPTimeStamper
except ImportError:
    from pyhanko.sign.timestamps.requests_client import HTTPTimeStamper

from minihsm import job_queue
from signing import dev_pki

POLL_INTERVAL = 2
SIGN_TIMEOUT  = 120
DEFAULT_STAMP_TEXT = "Firmado digitalmente\npor %(signer)s\n%(ts)s"
import os as _os
_FONT_CANDIDATES = [
    "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",  # metrica de Arial
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",                       # respaldo
]
FONT_PATH = next((f for f in _FONT_CANDIDATES if _os.path.exists(f)), _FONT_CANDIDATES[-1])


_TXT_MAP = {
    "—": "-", "–": "-", "‒": "-", "―": "-",
    "‘": "'", "’": "'", "‚": "'", "‛": "'",
    "“": '"', "”": '"', "„": '"', "‟": '"',
    "…": "...", "•": "-", " ": " ", "™": "(TM)", "®": "(R)",
}


def _sanitize_latin1(text):
    """Aproxima caracteres tipograficos fuera de Latin-1 (guion largo, comillas
    curvas...) para que la fuente estandar no pase a UTF-16 (BOM + letras
    espaciadas). Los acentos del espanol estan en Latin-1, se conservan."""
    if not text:
        return text
    for k, v in _TXT_MAP.items():
        text = text.replace(k, v)
    return text.encode("latin-1", "replace").decode("latin-1")


class PollingSigner(signers.Signer):
    """pyhanko Signer que delega la firma al miniHSM por la cola de polling."""

    def __init__(self, device_id: str):
        self.device_id = device_id
        store = SimpleCertificateStore()
        store.register(dev_pki.ca_cert_a1())
        super().__init__(signing_cert=dev_pki.device_cert_a1(device_id),
                         cert_registry=store)

    async def async_sign_raw(self, data: bytes, digest_algorithm: str,
                             dry_run: bool = False) -> bytes:
        if dry_run:
            return bytes(72)
        algo = str(digest_algorithm).lower().replace("-", "")
        if algo != "sha256":
            raise NotImplementedError(
                f"el miniHSM (curva P-256) solo soporta SHA-256; se solicito {digest_algorithm}")
        digest = hashlib.sha256(data).hexdigest()
        rid = job_queue.enqueue(self.device_id, digest)
        waited = 0
        while waited < SIGN_TIMEOUT:
            await asyncio.sleep(POLL_INTERVAL)
            waited += POLL_INTERVAL
            j = job_queue.get(self.device_id, rid)
            if j and j["status"] == job_queue.DONE:
                return bytes.fromhex(j["result"]["signature"])
            if j and j["status"] == job_queue.ERROR:
                raise RuntimeError(f"device reporto error: {j['result']}")
        raise TimeoutError("el device no firmo dentro del tiempo limite")


def _resolve_img_width(image_width, w_box, default_frac=0.40):
    """Ancho de la imagen en PUNTOS (0..w_box) en modo 'left'. Acepta "NN%"
    (fraccion del box) o "NN"/"NNpx" (puntos). Sin clamp a 10/90: 0 y w_box son
    validos para los casos extremos (solo texto / solo imagen)."""
    if image_width is None or str(image_width).strip() == "":
        return default_frac * w_box
    raw = str(image_width).strip().lower().replace("px", "").replace(" ", "")
    try:
        w = (float(raw[:-1]) / 100.0) * w_box if raw.endswith("%") else float(raw)
    except ValueError:
        return default_frac * w_box
    return max(0.0, min(w, w_box))


def _build_stamp_style(stamp_text, bg, image_opacity, image_mode, box,
                       text_opacity=1.0, border=True, border_width=3, image_width=None):
    """TextStampStyle SIN padding interno (margenes 0) y con la imagen escalada
    preservando su proporcion (SHRINK_TO_FIT). image_mode 'left' reparte
    imagen(izq)/texto(der) segun image_width; extremos >=98% solo imagen, <=2%
    solo texto. Todos los margenes/dimensiones son ENTEROS (pyhanko usa Fraction)."""
    tb = dict(
        box_layout_rule=SimpleBoxLayoutRule(
            x_align=AxisAlignment.ALIGN_MIN, y_align=AxisAlignment.ALIGN_MID,
            margins=Margins(left=0, right=0, top=0, bottom=0),
            inner_content_scaling=InnerScaling.SHRINK_TO_FIT))
    if text_opacity is not None and float(text_opacity) < 1.0:
        g = round(1.0 - float(text_opacity), 3)
        tb["text_color"] = (g, g, g)
    text_box_style = TextBoxStyle(**tb)

    w_box = int(abs(box[2] - box[0])) if box else 0

    side_by_side = bool(bg is not None and image_mode == "left" and box)
    img_w = 0
    if side_by_side:
        img_w = int(round(_resolve_img_width(image_width, w_box)))
        if img_w >= int(0.98 * w_box):       # solo imagen
            stamp_text = ""
            side_by_side = False
        elif img_w <= int(0.02 * w_box):     # solo texto
            bg = None
            side_by_side = False

    kw = dict(stamp_text=stamp_text, background=bg,
              background_opacity=image_opacity, text_box_style=text_box_style,
              border_width=(int(border_width) if border else 0))

    GAP = 8
    if side_by_side:
        kw["background_layout"] = SimpleBoxLayoutRule(
            x_align=AxisAlignment.ALIGN_MIN, y_align=AxisAlignment.ALIGN_MID,
            margins=Margins(left=0, right=max(0, w_box - img_w), top=0, bottom=0),
            inner_content_scaling=InnerScaling.SHRINK_TO_FIT)
        kw["inner_content_layout"] = SimpleBoxLayoutRule(
            x_align=AxisAlignment.ALIGN_MIN, y_align=AxisAlignment.ALIGN_MID,
            margins=Margins(left=img_w + GAP, right=0, top=0, bottom=0),
            inner_content_scaling=InnerScaling.SHRINK_TO_FIT)
    else:
        if bg is not None:
            kw["background_layout"] = SimpleBoxLayoutRule(
                x_align=AxisAlignment.ALIGN_MID, y_align=AxisAlignment.ALIGN_MID,
                margins=Margins(left=0, right=0, top=0, bottom=0),
                inner_content_scaling=InnerScaling.SHRINK_TO_FIT)
        kw["inner_content_layout"] = SimpleBoxLayoutRule(
            x_align=AxisAlignment.ALIGN_MIN, y_align=AxisAlignment.ALIGN_MID,
            margins=Margins(left=0, right=0, top=0, bottom=0),
            inner_content_scaling=InnerScaling.SHRINK_TO_FIT)
    return TextStampStyle(**kw)


def _unique_field_name(w):
    """Nombre de campo de firma libre: evita 'Signature1 already filled' y habilita
    firmas multiples. Detecta los campos ya presentes y toma el siguiente Signature{n}."""
    taken = set()
    try:
        for item in enumerate_sig_fields(w):
            nm = item[0] if isinstance(item, (tuple, list)) else getattr(item, "name", None)
            if nm:
                taken.add(str(nm))
    except Exception:
        pass
    n = 1
    while f"Signature{n}" in taken:
        n += 1
    return f"Signature{n}"


async def sign_pdf_bytes(
    pdf_bytes: bytes,
    device_id: str,
    *,
    name: str | None = None,
    reason: str = "Firma electronica miniHSM (Xami)",
    location: str = "Peru",
    contact: str | None = None,
    visible: bool = False,
    page: int = 0,
    box=None,
    stamp_image_bytes: bytes | None = None,
    stamp_text: str | None = None,
    image_opacity: float = 0.5,
    text_opacity: float = 1.0,
    image_mode: str = "left",            # left (default) | background
    image_width=None,                    # ancho img en modo left: '40%' o '120' (pts)
    border: bool = True,
    border_width: int = 3,
    certify: bool = False,
    certify_level: int = 1,               # 1 NO_CHANGES | 2 FILL_FORMS | 3 ANNOTATE (solo certify)
    tsa_url: str | None = None,          # RFC 3161 -> PAdES-T
) -> bytes:
    signer = PollingSigner(device_id)
    w = IncrementalPdfFileWriter(io.BytesIO(pdf_bytes), strict=False)

    field_name = _unique_field_name(w)   # unico -> habilita firmas multiples

    field_spec = (SigFieldSpec(field_name, on_page=page, box=box)
                  if visible else SigFieldSpec(field_name))

    # DocMDP: approval NO lleva restriccion; certify usa el nivel pedido (1/2/3)
    _MDP = {1: MDPPerm.NO_CHANGES, 2: MDPPerm.FILL_FORMS, 3: MDPPerm.ANNOTATE}
    docmdp = _MDP.get(int(certify_level), MDPPerm.NO_CHANGES) if certify else None

    meta = PdfSignatureMetadata(
        field_name=field_name,
        name=(name or f"MiniHSM-{device_id}"),
        reason=reason, location=location, contact_info=contact,
        certify=certify,
        subfilter=SigSeedSubFilter.PADES,
        docmdp_permissions=docmdp,
    )

    stamp_style = None
    if visible:
        bg = None
        if stamp_image_bytes:
            from PIL import Image
            bg = PdfImage(Image.open(io.BytesIO(stamp_image_bytes)))
        text = stamp_text if stamp_text is not None else DEFAULT_STAMP_TEXT
        text = _sanitize_latin1(text)
        stamp_style = _build_stamp_style(
            text, bg, image_opacity, image_mode, box,
            text_opacity=text_opacity, border=border, border_width=border_width,
            image_width=image_width)

    timestamper = HTTPTimeStamper(tsa_url) if tsa_url else None
    pdf_signer = signers.PdfSigner(
        meta, signer=signer, stamp_style=stamp_style,
        new_field_spec=field_spec, timestamper=timestamper)
    out = io.BytesIO()
    await pdf_signer.async_sign_pdf(w, output=out)
    return out.getvalue()
