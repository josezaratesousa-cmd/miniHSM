"""Firma PAdES con el miniHSM via la cola de polling (Bloque 8 Forma 2).

El server NO alcanza al device (NAT). async_sign_raw encola el digest en
job_queue y espera (async) a que el device lo recoja en su heartbeat y devuelva
la firma. Usa asyncio.sleep para no bloquear el event loop -> mientras esperamos,
el heartbeat del device entra y deposita el resultado en la misma cola.

Soporta:
  - firma INVISIBLE (default) o VISIBLE (sello con imagen/texto en pagina+box)
  - modo APPROVAL (default, se pueden agregar mas firmas) o CERTIFY (sella el
    documento: DocMDP NO_CHANGES, no se permite firmar/alterar mas)
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

from minihsm import job_queue
from signing import dev_pki

POLL_INTERVAL = 2     # segundos entre consultas a la cola
SIGN_TIMEOUT  = 120   # segundos maximos esperando al device


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


async def sign_pdf_bytes(
    pdf_bytes: bytes,
    device_id: str,
    *,
    reason: str = "Firma electronica miniHSM (Xami)",
    location: str = "Peru",
    visible: bool = False,
    page: int = 0,                       # 0-indexed
    box=None,                            # (x1, y1, x2, y2) en puntos PDF
    stamp_image_bytes: bytes | None = None,
    stamp_text: str | None = None,
    image_opacity: float = 0.5,
    certify: bool = False,
) -> bytes:
    """Firma un PDF (bytes) con PAdES usando el device. Devuelve bytes firmados."""
    signer = PollingSigner(device_id)
    w = IncrementalPdfFileWriter(io.BytesIO(pdf_bytes), strict=False)

    if visible:
        field_spec = SigFieldSpec("Signature1", on_page=page, box=box)
    else:
        field_spec = SigFieldSpec("Signature1")

    meta = PdfSignatureMetadata(
        field_name="Signature1", reason=reason, location=location,
        name=f"MiniHSM-{device_id}",
        certify=certify,
        docmdp_permissions=MDPPerm.NO_CHANGES if certify else MDPPerm.FILL_FORMS,
    )

    stamp_style = None
    if visible:
        bg = None
        if stamp_image_bytes:
            from PIL import Image
            bg = PdfImage(Image.open(io.BytesIO(stamp_image_bytes)))
        default_text = "Firmado digitalmente\npor %(signer)s\n%(ts)s"
        stamp_style = TextStampStyle(
            stamp_text=(default_text if stamp_text is None else stamp_text),
            background=bg,
            background_opacity=image_opacity,
        )

    pdf_signer = signers.PdfSigner(
        meta, signer=signer, stamp_style=stamp_style, new_field_spec=field_spec)
    out = io.BytesIO()
    await pdf_signer.async_sign_pdf(w, output=out)
    return out.getvalue()
