"""Bloque 8 - Forma 2: /v1/signatures/pdf

Subes un PDF -> el miniHSM lo firma (PAdES via cola de polling) -> descargas el
PDF firmado. Patron asincrono (evita timeouts del proxy mientras el device
pollea): POST encola y devuelve requestId; el cliente consulta el estado y
descarga cuando esta DONE.
"""
import asyncio
from fastapi import APIRouter, UploadFile, File, Form, HTTPException
from fastapi.responses import Response

from signing import pades_polling, dev_pki
from minihsm import pdf_jobs
from minihsm.registry import registry

router = APIRouter(prefix="/v1/signatures", tags=["signatures"])


def _resolve_device(device_id: str | None) -> str:
    if device_id:
        return device_id
    online = registry.online_devices()
    if not online:
        raise HTTPException(503, "no hay ningun device online en este momento")
    return online[0].device_id


async def _run(pid: str, pdf_bytes: bytes, device_id: str,
               reason: str, location: str):
    try:
        signed = await pades_polling.sign_pdf_bytes(
            pdf_bytes, device_id, reason=reason, location=location)
        pdf_jobs.set_done(pid, signed)
    except Exception as e:  # noqa: BLE001
        pdf_jobs.set_error(pid, e)


@router.post("/pdf")
async def sign_pdf(
    file:      UploadFile = File(...),
    device_id: str = Form(None),
    reason:    str = Form("Firma electronica miniHSM (Xami)"),
    location:  str = Form("Peru"),
):
    pdf_bytes = await file.read()
    if pdf_bytes[:4] != b"%PDF":
        raise HTTPException(400, "el archivo no parece un PDF")
    dev = _resolve_device(device_id)
    pid = pdf_jobs.create(file.filename or "documento.pdf")
    asyncio.create_task(_run(pid, pdf_bytes, dev, reason, location))
    return {"requestId": pid, "status": "processing", "deviceId": dev}


@router.get("/pdf/{pid}")
def status(pid: str):
    t = pdf_jobs.get(pid)
    if not t:
        raise HTTPException(404, "requestId no existe")
    return {"requestId": pid, "status": t["status"],
            "filename": t["filename"], "error": t["error"]}


@router.get("/pdf/{pid}/download")
def download(pid: str):
    t = pdf_jobs.get(pid)
    if not t:
        raise HTTPException(404, "requestId no existe")
    if t["status"] != pdf_jobs.DONE:
        raise HTTPException(409, f"aun no esta listo (estado: {t['status']})")
    return Response(
        content=t["signed_pdf"], media_type="application/pdf",
        headers={"Content-Disposition":
                 f'attachment; filename="firmado_{t["filename"]}"'})


@router.get("/ca.pem")
def ca_pem():
    return Response(
        content=dev_pki.ca_cert_pem(), media_type="application/x-pem-file",
        headers={"Content-Disposition": 'attachment; filename="xami_dev_ca.pem"'})
