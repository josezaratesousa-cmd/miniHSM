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
from pyhanko.sign.fields import SigFieldSpec, MDPPerm
from pyhanko.sign.signers.pdf_signer import PdfSignatureMetadata
from pyhanko.pdf_utils.incremental_writer import IncrementalPdfFileWriter
from pyhanko_certvalidator.registry import SimpleCertificateStore
from pyhanko.stamp import TextStampStyle
from pyhanko.pdf_utils.images import PdfImage
from pyhanko.pdf_utils.text import TextBoxStyle
from pyhanko.pdf_utils.layout import (
    SimpleBoxLayoutRule, AxisAlignment, Margins, InnerScaling)
try:
    from pyhanko.sign.timestamps import HTTPTimeStamper
except ImportError:
    from pyhanko.sign.timestamps.requests_client import HTTPTimeStamper

from minihsm import job_queue
from signing import dev_pki

POLL_INTERVAL = 2
SIGN_TIMEOUT  = 120
DEFAULT_STAMP_TEXT = "Firmado digitalmente\npor %(signer)s\n%(ts)s"


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


def _resolve_img_width(spec, w_box, default_frac=0.40):
    """Ancho de la imagen en modo 'left'. spec: None -> 40% del box; "NN%" ->
    fraccion del box; "NN" o "NNpx" -> NN puntos PDF. Se acota a [10%, 90%]."""
    if spec is None or str(spec).strip() == "":
        return default_frac * w_box
    raw = str(spec).strip().lower().replace("px", "").replace(" ", "")
    try:
        if raw.endswith("%"):
            w = (float(raw[:-1]) / 100.0) * w_box
        else:
            w = float(raw)
    except ValueError:
        return default_frac * w_box
    return max(0.10 * w_box, min(w, 0.90 * w_box))


def _build_stamp_style(stamp_text, bg, image_opacity, image_mode, box,
                       text_opacity=1.0, border=True, border_width=3, image_width=None):
    """Arma el TextStampStyle. image_mode: 'background' (fondo) | 'left' (imagen
    a la izquierda, texto a la derecha). text_opacity<1 atenua el texto via color
    (pyhanko no soporta alpha real en texto). border/border_width: marco de toda
    la firma (imagen + texto)."""
    tb = {}
    if text_opacity is not None and float(text_opacity) < 1.0:
        g = round(1.0 - float(text_opacity), 3)
        tb["text_color"] = (g, g, g)
    text_box_style = TextBoxStyle(**tb) if tb else TextBoxStyle()
    kw = dict(stamp_text=stamp_text, background=bg, background_opacity=image_opacity,
              text_box_style=text_box_style,
              border_width=(int(border_width) if border else 0))
    if bg is not None and image_mode == "left" and box:
        w_box = abs(box[2] - box[0])
        img_w = _resolve_img_width(image_width, w_box)
        kw["background_opacity"] = max(image_opacity, 0.9)
        kw["background_layout"] = SimpleBoxLayoutRule(
            x_align=AxisAlignment.ALIGN_MIN, y_align=AxisAlignment.ALIGN_MID,
            margins=Margins(left=4, right=w_box - img_w, top=4, bottom=4),
            inner_content_scaling=InnerScaling.SHRINK_TO_FIT)
        kw["inner_content_layout"] = SimpleBoxLayoutRule(
            x_align=AxisAlignment.ALIGN_MIN, y_align=AxisAlignment.ALIGN_MID,
            margins=Margins(left=img_w + 10, right=4, top=4, bottom=4))
    return TextStampStyle(**kw)


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
    image_mode: str = "background",      # background | left
    image_width=None,                    # ancho img en modo left: '40%' o '120' (pts)
    border: bool = True,
    border_width: int = 3,
    certify: bool = False,
    tsa_url: str | None = None,          # RFC 3161 -> PAdES-T
) -> bytes:
    signer = PollingSigner(device_id)
    w = IncrementalPdfFileWriter(io.BytesIO(pdf_bytes), strict=False)

    field_spec = (SigFieldSpec("Signature1", on_page=page, box=box)
                  if visible else SigFieldSpec("Signature1"))

    meta = PdfSignatureMetadata(
        field_name="Signature1",
        name=(name or f"MiniHSM-{device_id}"),
        reason=reason, location=location, contact_info=contact,
        certify=certify,
        docmdp_permissions=MDPPerm.NO_CHANGES if certify else MDPPerm.FILL_FORMS,
    )

    stamp_style = None
    if visible:
        bg = None
        if stamp_image_bytes:
            from PIL import Image
            bg = PdfImage(Image.open(io.BytesIO(stamp_image_bytes)))
        text = stamp_text if stamp_text is not None else DEFAULT_STAMP_TEXT
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
