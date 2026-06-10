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


# ── Emparejamiento (match) — Bloque 9 ─────────────────────────────────────────

from minihsm import sold_devices, device_secrets
from minihsm.crypto_match import verify_proof_of_possession, ecies_encrypt


class MatchRequest(BaseModel):
    deviceId:  str
    pubkey:    str          # EC P-256 uncompressed, hex (04||X||Y)
    timestamp: int
    nonce:     str
    signature: str          # firma DER (hex) de "<deviceId>:<timestamp>:<nonce>"
    ip:        str = ""
    model:     str = "A1"


class MatchResponse(BaseModel):
    status:          str
    deviceId:        str
    message:         str
    secretsEncrypted: str   # blob ECIES (hex): contiene el HMAC secret cifrado con la pubkey del device


@router.post("/match", response_model=MatchResponse)
def match(req: Request, body: MatchRequest):
    """
    Emparejamiento de un Xami con el server (Bloque 9, Capa 1).
    Verifica: deviceID vendido + proof of possession. Registra la pubkey (TOFU A1).
    La entrega de secretos cifrados (ECIES) se agrega en Capa 2.
    """
    # 1. Bloqueado?
    if sold_devices.is_blocked(body.deviceId):
        raise HTTPException(403, f"Device {body.deviceId} is blocked")

    # 2. Es un device vendido/legitimo?
    if not sold_devices.is_sold(body.deviceId):
        raise HTTPException(403, f"Device {body.deviceId} not in sold list")

    # 3. Proof of possession: la firma valida contra la pubkey declarada?
    challenge = f"{body.deviceId}:{body.timestamp}:{body.nonce}".encode()
    try:
        sig_der = bytes.fromhex(body.signature)
    except ValueError:
        raise HTTPException(400, "signature must be hex")

    if not verify_proof_of_possession(body.pubkey, challenge, sig_der):
        raise HTTPException(401, "Invalid proof of possession")

    # 4. TOFU (Trust On First Use) para A1: si no hay pubkey registrada, la guardamos.
    #    Si ya hay una y NO coincide -> posible suplantacion -> rechazar.
    known = sold_devices.get_pubkey(body.deviceId)
    if known is None:
        sold_devices.set_pubkey(body.deviceId, body.pubkey)
        log.info(f"Match: {body.deviceId} pubkey registrada (TOFU)")
    elif known != body.pubkey:
        raise HTTPException(409, "pubkey mismatch — posible suplantacion")

    # Registrar IP/estado
    ip = (req.headers.get("X-Forwarded-For", "").split(",")[0].strip()
          or req.client.host)
    registry.register(device_id=body.deviceId, ip=ip)

    # 5. Generar el HMAC secret (32 bytes), guardarlo, y cifrarlo con ECIES para el device
    hmac_secret = device_secrets.generate_hmac_secret()
    device_secrets.set_secret(body.deviceId, hmac_secret.hex())
    blob = ecies_encrypt(body.pubkey, hmac_secret)

    log.info(f"Match OK: {body.deviceId} @ {ip} (model={body.model}) - secret entregado")
    return MatchResponse(
        status           = "ok",
        deviceId         = body.deviceId,
        message          = "matched - secret entregado cifrado",
        secretsEncrypted = blob,
    )
