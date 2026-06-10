"""
ServerHSM Optimizer — FastAPI
"""

import hashlib
import logging
import os
from typing import Optional
from pathlib import Path

from fastapi import FastAPI, HTTPException, UploadFile, File, Form, Request
from fastapi.responses import Response
from pydantic import BaseModel

from minihsm.client import MiniHSMClient
from minihsm.registry import registry
from minihsm import device_secrets
from signing.pades import sign_pdf
from signing.xades import sign_xml
from signing.cades import sign_data, verify_cades
from api.devices import router as devices_router

logging.basicConfig(level=logging.INFO)
log = logging.getLogger("serverHSM")

# ── Config ────────────────────────────────────────────────────────────────────
MINIHSM_HOST   = os.getenv("MINIHSM_HOST",   "")
MINIHSM_PORT   = int(os.getenv("MINIHSM_PORT", "80"))
MINIHSM_SECRET = os.getenv("MINIHSM_SECRET", "")
MINIHSM_DEVICE = os.getenv("MINIHSM_DEVICE", "")  # deviceId fijo (opcional)

app = FastAPI(
    title       = "ServerHSM Optimizer",
    description = "Servicio de firma digital usando miniHSM ESP32-S3",
    version     = "2.0.0",
    root_path   = "/serverHSM/api",
)

app.include_router(devices_router)


def get_client() -> MiniHSMClient:
    """
    Obtiene el cliente HTTP para el miniHSM.
    Prioridad:
      1. IP del registro (heartbeat dinámico)
      2. MINIHSM_HOST del .env (IP fija configurada)
    """
    host = MINIHSM_HOST

    # Si hay un deviceId configurado, usar la IP del registro
    if MINIHSM_DEVICE:
        url = registry.get_url(MINIHSM_DEVICE)
        if url:
            entry = registry.get(MINIHSM_DEVICE)
            host  = entry.ip
            log.debug(f"Using registered IP for {MINIHSM_DEVICE}: {host}")
        elif not host:
            raise HTTPException(
                503,
                detail={
                    "error":   "miniHSM unreachable",
                    "message": "No heartbeat received yet. "
                               "The miniHSM will register its IP in the next 5 minutes.",
                    "deviceId": MINIHSM_DEVICE,
                }
            )

    if not host:
        raise HTTPException(
            503,
            detail="MINIHSM_HOST not configured and no device registered"
        )

    client = MiniHSMClient(host, MINIHSM_PORT)
    # Secret por-device (del match, Bloque 9); fallback al global del .env
    secret = device_secrets.get_secret(MINIHSM_DEVICE) if MINIHSM_DEVICE else None
    if not secret:
        secret = MINIHSM_SECRET
    if secret:
        client.set_secret(secret)
    return client


# ── Health / Device ────────────────────────────────────────────────────────────

@app.get("/health")
def health():
    online = registry.online_devices()
    result = {"optimizer": "ok", "registeredDevices": len(online)}
    try:
        result["miniHSM"] = get_client().health()
    except Exception as e:
        result["miniHSM"] = {"error": str(e)}
    return result

@app.get("/device")
def device_info():
    return get_client().get_device_info().__dict__

@app.get("/cert")
def get_cert():
    return {"certificate_pem": get_client().get_certificate_pem()}

@app.get("/csr")
def get_csr():
    return {"csr_pem": get_client().get_csr_pem()}

@app.post("/cert")
def load_ca_cert(req: "LoadCertRequest"):
    ok = get_client().load_ca_certificate(req.certificate_pem)
    if not ok:
        raise HTTPException(400, "Certificate rejected by device")
    return {"status": "PROVISIONED"}


# ── Modelos ───────────────────────────────────────────────────────────────────

class DigestSignRequest(BaseModel):
    digest_hex: str
    request_id: Optional[str] = None

class LoadCertRequest(BaseModel):
    certificate_pem: str


# ── Firma digest ──────────────────────────────────────────────────────────────

@app.post("/sign/digest")
def sign_digest(req: DigestSignRequest):
    if len(req.digest_hex) != 64:
        raise HTTPException(400, "digest_hex must be 64 hex chars")
    result = get_client().sign(req.digest_hex, req.request_id)
    return {
        "requestId":      result.request_id,
        "signatureHex":   result.signature_hex,
        "certificatePem": result.certificate_pem,
        "deviceId":       result.device_id,
        "algorithm":      "ECDSA-P256-SHA256-DER",
    }


# ── PAdES ─────────────────────────────────────────────────────────────────────

@app.post("/sign/pdf")
async def sign_pdf_endpoint(
    file:     UploadFile = File(...),
    reason:   str        = Form("Firma electronica ServerHSM"),
    location: str        = Form("Peru"),
    tsa_url:  str        = Form(None),
):
    pdf_bytes = await file.read()
    in_path   = Path(f"/tmp/hsm_in_{file.filename}")
    out_path  = Path(f"/tmp/hsm_out_{file.filename}")
    in_path.write_bytes(pdf_bytes)
    try:
        sign_pdf(in_path, out_path, get_client(), reason, location, tsa_url)
        return Response(
            content      = out_path.read_bytes(),
            media_type   = "application/pdf",
            headers      = {"Content-Disposition":
                            f'attachment; filename="signed_{file.filename}"'},
        )
    finally:
        in_path.unlink(missing_ok=True)
        out_path.unlink(missing_ok=True)


# ── XAdES ─────────────────────────────────────────────────────────────────────

@app.post("/sign/xml")
async def sign_xml_endpoint(file: UploadFile = File(...)):
    xml_bytes = await file.read()
    signed    = sign_xml(xml_bytes, get_client())
    return Response(
        content    = signed,
        media_type = "application/xml",
        headers    = {"Content-Disposition":
                      f'attachment; filename="signed_{file.filename}"'},
    )


# ── CAdES ─────────────────────────────────────────────────────────────────────

@app.post("/sign/data")
async def sign_data_endpoint(
    file:     UploadFile = File(...),
    detached: bool       = Form(True),
):
    data   = await file.read()
    signed = sign_data(data, get_client(), detached=detached)
    return Response(
        content    = signed,
        media_type = "application/pkcs7-signature",
        headers    = {"Content-Disposition": 'attachment; filename="signature.p7s"'},
    )


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8181)
