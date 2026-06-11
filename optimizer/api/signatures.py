"""Bloque 8 - Forma 2: /v1/signatures/pdf

Subes un PDF -> el miniHSM lo firma (PAdES via cola de polling) -> descargas el
PDF firmado. Patron asincrono (POST encola y devuelve requestId; el cliente
consulta el estado y descarga cuando esta DONE).

Soporta firma VISIBLE (imagen de sello + pagina + coordenadas + texto) y el modo
APPROVAL (se pueden agregar mas firmas) vs CERTIFY (sella el documento).
"""
import asyncio
from fastapi import APIRouter, UploadFile, File, Form, HTTPException
from fastapi.responses import Response

from signing import pades_polling, dev_pki
from minihsm import pdf_jobs
from minihsm.registry import registry

router = APIRouter(prefix="/v1/signatures", tags=["signatures"])

# box por defecto si piden firma visible sin coordenadas (esquina inferior izq.)
DEFAULT_BOX = (40, 40, 260, 120)


def _resolve_device(device_id: str | None) -> str:
    if device_id:
        return device_id
    online = registry.online_devices()
    if not online:
        raise HTTPException(503, "no hay ningun device online en este momento")
    return online[0].device_id


def _parse_box(box: str | None):
    if not box:
        return None
    try:
        parts = [int(float(v)) for v in box.replace(" ", "").split(",")]
        assert len(parts) == 4
        return tuple(parts)
    except Exception:
        raise HTTPException(400, "box debe ser 'x1,y1,x2,y2' (puntos PDF)")


async def _run(pid: str, pdf_bytes: bytes, device_id: str, kwargs: dict):
    try:
        signed = await pades_polling.sign_pdf_bytes(pdf_bytes, device_id, **kwargs)
        pdf_jobs.set_done(pid, signed)
    except Exception as e:  # noqa: BLE001
        pdf_jobs.set_error(pid, e)


@router.post("/pdf")
async def sign_pdf(
    file:          UploadFile = File(...),
    device_id:     str = Form(None),
    name:          str = Form(None),
    reason:        str = Form("Firma electronica miniHSM (Xami)"),
    location:      str = Form("Peru"),
    contact:       str = Form(None),
    visible:       bool = Form(False),
    page:          int = Form(1),           # 1-indexed para el usuario
    box:           str = Form(None),        # "x1,y1,x2,y2"
    stamp_text:    str = Form(None),
    stamp_source:  str = Form("attributes"),  # attributes | default | custom
    image_opacity: float = Form(0.5),
    text_opacity:  float = Form(1.0),
    image_mode:    str = Form("background"),   # background | left
    image_width:   str = Form(None),           # '40%' o '120' (pts) - solo modo left
    border:        bool = Form(True),
    border_width:  int = Form(2),
    tsa_url:       str = Form(None),           # RFC 3161 -> PAdES-T
    mode:          str = Form("approval"),     # approval | certify
    stamp_image:   UploadFile = File(None),
):
    pdf_bytes = await file.read()
    if pdf_bytes[:4] != b"%PDF":
        raise HTTPException(400, "el archivo no parece un PDF")

    mode = (mode or "approval").lower()
    if mode not in ("approval", "certify"):
        raise HTTPException(400, "mode debe ser 'approval' o 'certify'")

    box_tuple = _parse_box(box)
    if visible and box_tuple is None:
        box_tuple = DEFAULT_BOX

    img_bytes = await stamp_image.read() if stamp_image is not None else None

    # Contenido del sello visible (3 modos):
    #   attributes -> arma las lineas con los atributos del diccionario + fecha
    #   default    -> sello estandar del motor (firmante + fecha), generico
    #   custom     -> texto libre (stamp_text)
    if visible:
        src = (stamp_source or "default").lower()
        if src == "attributes":
            _l = [f"Firmado por: {name}" if name else "Firmado por: %(signer)s"]
            if reason:   _l.append(f"Razón: {reason}")
            if location: _l.append(f"Lugar: {location}")
            if contact:  _l.append(f"Contacto: {contact}")
            _l.append("Fecha: %(ts)s")
            stamp_text = "\n".join(_l)
        elif src == "default":
            stamp_text = None   # el motor usa su texto estandar

    dev = _resolve_device(device_id)
    kwargs = dict(
        name=name, reason=reason, location=location, contact=contact,
        visible=visible, page=max(0, page - 1), box=box_tuple,
        stamp_image_bytes=img_bytes, stamp_text=stamp_text,
        image_opacity=image_opacity, text_opacity=text_opacity,
        image_mode=image_mode, image_width=image_width,
        border=border, border_width=border_width,
        certify=(mode == "certify"), tsa_url=tsa_url,
    )
    pid = pdf_jobs.create(file.filename or "documento.pdf")
    asyncio.create_task(_run(pid, pdf_bytes, dev, kwargs))
    return {"requestId": pid, "status": "processing", "deviceId": dev,
            "visible": visible, "mode": mode}


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
