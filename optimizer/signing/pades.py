"""
PAdES Signer — firma PDFs usando el MiniHSM como backend criptografico.

Niveles soportados:
  PAdES-B-B  : firma basica con certificado embebido
  PAdES-B-T  : B-B + timestamp RFC 3161

El documento NUNCA sale del Optimizer hacia el MiniHSM.
Solo el hash SHA-256 del ByteRange se envia al dispositivo.
"""

import hashlib
import struct
from pathlib import Path
from typing import Union

from cryptography import x509
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
from cryptography.hazmat.backends import default_backend

from pyhanko.sign import signers, fields
from pyhanko.sign.fields import SigFieldSpec
from pyhanko.pdf_utils.incremental_writer import IncrementalPdfFileWriter
from pyhanko.sign.signers.pdf_signer import PdfSignatureMetadata
from pyhanko.sign.timestamps import HTTPTimeStamper

from minihsm.client import MiniHSMClient


class MiniHSMPdfSigner(signers.Signer):
    """
    Implementacion de pyhanko.Signer que delega la firma al MiniHSM.
    pyhanko calcula el ByteRange y el hash; nosotros firmamos con el chip.
    """

    def __init__(self, client: MiniHSMClient):
        self.client = client
        cert_pem    = client.get_certificate_pem()
        self._cert  = x509.load_pem_x509_certificate(
            cert_pem.encode(), default_backend())
        super().__init__()

    @property
    def signing_cert(self):
        return self._cert

    @property
    def cert_registry(self):
        from pyhanko.sign.general import SimpleCertificateStore
        store = SimpleCertificateStore()
        store.register(self._cert)
        return store

    async def async_sign_raw(self, data: bytes, digest_algorithm: str,
                              dry_run: bool = False) -> bytes:
        if dry_run:
            return bytes(CRYPTO_SIG_DER_MAX_SIZE)  # placeholder

        digest_hex = hashlib.sha256(data).hexdigest()
        result     = self.client.sign(digest_hex)
        return bytes.fromhex(result.signature_hex)


CRYPTO_SIG_DER_MAX_SIZE = 72


def sign_pdf(
    pdf_path: Union[str, Path],
    output_path: Union[str, Path],
    client: MiniHSMClient,
    reason: str = "Firma electronica MiniHSM",
    location: str = "Peru",
    tsa_url: str | None = None,
) -> Path:
    """
    Firma un PDF con PAdES-B-B (o PAdES-B-T si se provee tsa_url).

    Args:
        pdf_path:    PDF de entrada
        output_path: PDF firmado de salida
        client:      MiniHSMClient conectado al dispositivo
        reason:      Razon de firma (aparece en el visor de PDF)
        location:    Lugar de firma
        tsa_url:     URL de TSA para sello de tiempo RFC 3161 (opcional)

    Returns:
        Path al PDF firmado
    """
    pdf_path    = Path(pdf_path)
    output_path = Path(output_path)

    signer = MiniHSMPdfSigner(client)

    timestamper = None
    if tsa_url:
        timestamper = HTTPTimeStamper(tsa_url)

    with open(pdf_path, "rb") as f:
        writer = IncrementalPdfFileWriter(f)

        # Agregar campo de firma si no existe
        if not any(
            isinstance(field, fields.SignatureFormField)
            for field in writer.root["/AcroForm"].get("/Fields", [])
        ):
            fields.append_signature_field(
                writer,
                SigFieldSpec("Signature1", on_page=0)
            )

        meta = PdfSignatureMetadata(
            field_name    = "Signature1",
            reason        = reason,
            location      = location,
            name          = f"MiniHSM-{client.get_device_info().device_id}",
            certify       = False,
        )

        with open(output_path, "wb") as out:
            import asyncio
            asyncio.get_event_loop().run_until_complete(
                signers.async_sign_pdf(
                    writer,
                    meta,
                    signer     = signer,
                    timestamper= timestamper,
                    output     = out,
                )
            )

    return output_path


def verify_pdf(pdf_path: Union[str, Path]) -> list[dict]:
    """
    Verifica las firmas de un PDF firmado.
    Devuelve lista de resultados por firma.
    """
    from pyhanko.sign import validation
    from pyhanko.pdf_utils.reader import PdfFileReader

    results = []
    with open(pdf_path, "rb") as f:
        reader = PdfFileReader(f)
        for sig in validation.list_embedded_signatures(reader):
            try:
                status = validation.validate_pdf_signature(
                    sig, validation.RevocationInfoGatherer()
                )
                results.append({
                    "field":       sig.field_name,
                    "signer":      status.signing_cert.subject.rfc4514_string(),
                    "valid":       status.valid,
                    "trusted":     status.trusted,
                    "timestamp":   str(status.timestamp) if status.timestamp else None,
                })
            except Exception as e:
                results.append({"field": sig.field_name, "error": str(e)})

    return results
