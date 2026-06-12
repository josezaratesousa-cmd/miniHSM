"""
Servicio de custodia de certificados (.p12) — ceremonia de transferencia (Fase 4a).

El server SOLO orquesta: emite un secreto de ceremonia de un solo uso, entrega al
cliente la pubkey + fingerprint del chip (para anclar la confianza) y deja la ceremonia
pendiente para que el chip entre en modo AP (se entrega por heartbeat).
xami.run NUNCA ve el .p12, la passphrase ni la semilla TOTP (viajan cifrados al chip).
"""
import hashlib
import secrets as _secrets
import threading
import time

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from minihsm import sold_devices

router = APIRouter(prefix="/v1/credentials", tags=["credentials"])

_CEREMONY_TTL = 600  # segundos
_LOCK = threading.Lock()
_CEREMONIES: dict[str, dict] = {}   # deviceId -> {ceremonyId, secret, alias, expiresAt}


def _fingerprint(pubkey_hex: str) -> str:
    """SHA-256 (hex) de la pubkey del chip. El cliente lo compara contra el que muestra
    el propio chip en modo AP para detectar un impostor."""
    return hashlib.sha256(bytes.fromhex(pubkey_hex)).hexdigest()


class CeremonyStartReq(BaseModel):
    deviceId: str
    alias: str


@router.post("/ceremony/start")
def ceremony_start(body: CeremonyStartReq):
    pubkey = sold_devices.get_pubkey(body.deviceId)
    if not pubkey:
        raise HTTPException(409, "device sin pubkey (no emparejado)")
    secret = _secrets.token_hex(16)
    cid = "cer_" + _secrets.token_hex(8)
    with _LOCK:
        _CEREMONIES[body.deviceId] = {
            "ceremonyId": cid, "secret": secret,
            "alias": body.alias, "expiresAt": time.time() + _CEREMONY_TTL,
        }
    return {
        "ceremonyId":  cid,
        "deviceId":    body.deviceId,
        "alias":       body.alias,
        "pubkey":      pubkey,
        "fingerprint": _fingerprint(pubkey),
        "secret":      secret,
        "expiresIn":   _CEREMONY_TTL,
    }


def pop_pending_ceremony(device_id: str) -> dict | None:
    """Heartbeat: entrega (y consume) la ceremonia pendiente del device. Limpia expiradas."""
    with _LOCK:
        c = _CEREMONIES.get(device_id)
        if not c:
            return None
        if time.time() > c["expiresAt"]:
            _CEREMONIES.pop(device_id, None)
            return None
        return _CEREMONIES.pop(device_id)
