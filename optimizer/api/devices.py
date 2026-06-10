"""
Device registration & heartbeat endpoints.
El miniHSM llama aquí cada 5 minutos.
El serverHSM extrae la IP del request y la guarda.
"""

import logging
from fastapi import APIRouter, Request, HTTPException
from pydantic import BaseModel

from minihsm.registry import registry, DeviceEntry

log    = logging.getLogger("devices")
router = APIRouter(prefix="/devices", tags=["devices"])


class HeartbeatRequest(BaseModel):
    deviceId:  str
    token:     str
    timestamp: int
    nonce:     str
    firmware:  str = ""


class HeartbeatResponse(BaseModel):
    status:    str
    deviceId:  str
    ip:        str
    message:   str


@router.post("/heartbeat", response_model=HeartbeatResponse)
def heartbeat(req: Request, body: HeartbeatRequest):
    """
    Recibe el heartbeat del miniHSM.
    Extrae la IP pública del request (funciona con NAT y proxies).
    Guarda/actualiza la IP en el registro.
    """
    # Extraer IP real (funciona detrás de proxies/CDN)
    ip = (
        req.headers.get("X-Forwarded-For", "").split(",")[0].strip()
        or req.headers.get("X-Real-IP", "")
        or req.client.host
    )

    if not ip:
        raise HTTPException(400, "Cannot determine client IP")

    # TODO: validar token HMAC con el secret del device
    # Por ahora registramos y logueamos
    entry = registry.register(
        device_id = body.deviceId,
        ip        = ip,
        firmware  = body.firmware,
    )

    log.info(f"Heartbeat from {body.deviceId} @ {ip} (firmware={body.firmware})")

    return HeartbeatResponse(
        status   = "ok",
        deviceId = body.deviceId,
        ip       = ip,
        message  = "registered",
    )


@router.get("/")
def list_devices():
    """Lista todos los miniHSM registrados y su estado."""
    devices = registry.all_devices()
    return {
        "total":  len(devices),
        "online": len(registry.online_devices()),
        "devices": [
            {
                "deviceId":           d.device_id,
                "ip":                 d.ip,
                "online":             d.is_online,
                "firmware":           d.firmware,
                "secondsSinceHeart":  d.seconds_since_heartbeat,
            }
            for d in devices
        ]
    }


@router.get("/{device_id}")
def get_device(device_id: str):
    """Estado de un miniHSM específico."""
    entry = registry.get(device_id)
    if not entry:
        raise HTTPException(404, f"Device {device_id} not registered")
    return {
        "deviceId":          entry.device_id,
        "ip":                entry.ip,
        "online":            entry.is_online,
        "firmware":          entry.firmware,
        "baseUrl":           entry.base_url,
        "secondsSinceHeart": entry.seconds_since_heartbeat,
    }
