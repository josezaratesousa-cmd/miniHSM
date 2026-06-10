"""
Optimizer API — FastAPI
Expone endpoints de alto nivel para firmar documentos usando el MiniHSM.
"""

import hashlib
import base64
from pathlib import Path
from typing import Optional

from fastapi import FastAPI, HTTPException, UploadFile, File, Form
from fastapi.responses import Response
from pydantic import BaseModel

from minihsm.client import MiniHSMClient
from signing.pades import sign_pdf
from signing.xades import sign_xml
from signing.cades import sign_data, verify_cades

# ── Config ────────────────────────────────────────────────────────────────────

import os
MINIHSM_HOST   = os.getenv("MINIHSM_HOST",   "192.168.1.100")
MINIHSM_PORT   = int(os.getenv("MINIHSM_PORT", "80"))
MINIHSM_SECRET = os.getenv("MINIHSM_SECRET", "")  # hex del HMAC secret

app = FastAPI(
    title       = "MiniHSM Optimizer",
    description = "Servicio de firma digital usando MiniHSM ESP32-S3",
    version     = "2.0.0",
)


def get_client() -> MiniHSMClient:
    client = MiniHSMClient(MINIHSM_HOST, MINIHSM_PORT)
    if MINIHSM_SECRET:
        client.set_secret(MINIHSM_SECRET)
    return client


# ── Modelos ───────────────────────────────────────────────────────────────────

class DigestSignRequest(BaseModel):
    digest_hex: str
    request_id: Optional[str] = None

class DigestSignResponse(BaseModel):
    request_id:     str
    signature_hex:  str
    certificate_pem:str
    device_id:      str
    algorithm:      str = "ECDSA-P256-SHA256-DER"

class LoadCertRequest(BaseModel):
    certificate_pem: str


# ── Endpoints generales ───────────────────────────────────────────────────────

@app.get("/health")
def health():
    try:
        client = get_client()
        device = client.health()
        return {"optimizer": "ok", "device": device}
    except Exception as e:
        raise HTTPException(502, f"Cannot reach MiniHSM: {e}")

@app.get("/device")
def device_info():
    return get_client().get_device_info().__dict__

@app.get("/cert")
def get_cert():
    return {"certificate_pem": get_client().get_certificate_pem()}

@app.get("/csr")
def get_csr():
    """Obtiene el CSR para enviarlo a una CA."""
    return {"csr_pem": get_client().get_csr_pem()}

@app.post("/cert")
def load_ca_cert(req: LoadCertRequest):
    """Carga el certificado firmado por la CA tras la ceremonia."""
    ok = get_client().load_ca_certificate(req.certificate_pem)
    if not ok:
        raise HTTPException(400, "Certificate rejected by device")
    return {"status": "PROVISIONED"}


# ── Firma de digest (cualquier tipo) ─────────────────────────────────────────

@app.post("/sign/digest", response_model=DigestSignResponse)
def sign_digest(req: DigestSignRequest):
    """
    Firma un digest SHA-256 directamente.
    Usar cuando el Optimizer externo ya calculó el hash.
    """
    if len(req.digest_hex) != 64:
        raise HTTPException(400, "digest_hex must be 64 hex chars")
    result = get_client().sign(req.digest_hex, req.request_id)
    return DigestSignResponse(
        request_id      = result.request_id,
        signature_hex   = result.signature_hex,
        certificate_pem = result.certificate_pem,
        device_id       = result.device_id,
    )


# ── PAdES — firma de PDF ──────────────────────────────────────────────────────

@app.post("/sign/pdf")
async def sign_pdf_endpoint(
    file:     UploadFile = File(...),
    reason:   str        = Form("Firma electronica MiniHSM"),
    location: str        = Form("Peru"),
    tsa_url:  str        = Form(None),
):
    """
    Firma un PDF con PAdES-B-B.
    Sube el PDF como multipart/form-data, recibe el PDF firmado.
    """
    pdf_bytes = await file.read()
    in_path   = Path(f"/tmp/minihsm_in_{file.filename}")
    out_path  = Path(f"/tmp/minihsm_out_{file.filename}")

    in_path.write_bytes(pdf_bytes)

    try:
        sign_pdf(
            pdf_path    = in_path,
            output_path = out_path,
            client      = get_client(),
            reason      = reason,
            location    = location,
            tsa_url     = tsa_url,
        )
        signed_bytes = out_path.read_bytes()
        return Response(
            content      = signed_bytes,
            media_type   = "application/pdf",
            headers      = {"Content-Disposition":
                            f'attachment; filename="signed_{file.filename}"'},
        )
    finally:
        in_path.unlink(missing_ok=True)
        out_path.unlink(missing_ok=True)


# ── XAdES — firma de XML / facturas ──────────────────────────────────────────

@app.post("/sign/xml")
async def sign_xml_endpoint(
    file: UploadFile = File(...),
):
    """
    Firma un XML con XAdES-BES (compatible SUNAT Peru).
    Sube el XML, recibe el XML firmado.
    """
    xml_bytes = await file.read()
    signed    = sign_xml(xml_bytes, get_client())
    return Response(
        content    = signed,
        media_type = "application/xml",
        headers    = {"Content-Disposition":
                      f'attachment; filename="signed_{file.filename}"'},
    )


# ── CAdES — firma de datos genéricos ─────────────────────────────────────────

@app.post("/sign/data")
async def sign_data_endpoint(
    file:     UploadFile = File(...),
    detached: bool       = Form(True),
):
    """
    Firma datos arbitrarios con CAdES-BES.
    Devuelve el contenedor CMS/PKCS#7 en DER.
    """
    data   = await file.read()
    signed = sign_data(data, get_client(), detached=detached)
    return Response(
        content    = signed,
        media_type = "application/pkcs7-signature",
        headers    = {"Content-Disposition": "attachment; filename=\"signature.p7s\""},
    )


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)
