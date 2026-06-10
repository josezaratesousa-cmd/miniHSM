"""
Almacen de secretos por device (el server recuerda que secret le dio a cada Xami).

El server genera el HMAC secret en el match y se lo entrega cifrado al device.
Necesita guardarlo para luego generar tokens validos para ese device.

Por ahora archivo JSON. En produccion: BD cifrada / vault.
"""

import json
import os
import threading
import secrets as _secrets

_LOCK = threading.Lock()
_PATH = os.getenv("DEVICE_SECRETS_PATH",
                  os.path.join(os.path.dirname(__file__), "device_secrets.json"))


def _load() -> dict:
    if not os.path.exists(_PATH):
        return {}
    try:
        with open(_PATH) as f:
            return json.load(f)
    except Exception:
        return {}


def _save(data: dict) -> None:
    tmp = _PATH + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, indent=2)
    os.replace(tmp, _PATH)
    os.chmod(_PATH, 0o600)


def generate_hmac_secret() -> bytes:
    """Genera un HMAC secret aleatorio de 32 bytes."""
    return _secrets.token_bytes(32)


def set_secret(device_id: str, secret_hex: str) -> None:
    with _LOCK:
        data = _load()
        data[device_id] = {"hmac_secret": secret_hex}
        _save(data)


def get_secret(device_id: str) -> str | None:
    with _LOCK:
        d = _load().get(device_id)
        return d.get("hmac_secret") if d else None
