"""Firma PAdES con el miniHSM via la cola de polling (Bloque 8 Forma 2).

El server NO alcanza al device (NAT). async_sign_raw encola el digest en
job_queue y espera (async) a que el device lo recoja en su heartbeat y devuelva
la firma. Usa asyncio.sleep para no bloquear el event loop -> mientras esperamos,
el heartbeat del device entra y deposita el resultado en la misma cola.
"""
import hashlib
import asyncio
import io

from pyhanko.sign import signers, fields
from pyhanko.sign.fields import SigFieldSpec
from pyhanko.sign.signers.pdf_signer import PdfSignatureMetadata
from pyhanko.pdf_utils.incremental_writer import IncrementalPdfFileWriter
from pyhanko_certvalidator.registry import SimpleCertificateStore

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


async def sign_pdf_bytes(pdf_bytes: bytes, device_id: str,
                         reason: str = "Firma electronica miniHSM (Xami)",
                         location: str = "Peru") -> bytes:
    """Firma un PDF (bytes) con PAdES-B-B usando el device. Devuelve bytes firmados."""
    signer = PollingSigner(device_id)
    w = IncrementalPdfFileWriter(io.BytesIO(pdf_bytes))
    fields.append_signature_field(w, SigFieldSpec("Signature1"))  # invisible
    meta = PdfSignatureMetadata(
        field_name="Signature1", reason=reason, location=location,
        name=f"MiniHSM-{device_id}", certify=False)
    out = io.BytesIO()
    await signers.async_sign_pdf(w, meta, signer=signer, output=out)
    return out.getvalue()
