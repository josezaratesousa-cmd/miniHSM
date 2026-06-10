"""
Lista de dispositivos vendidos / legitimos.

Modelo de confianza A1 (sin chip): el server confia en la pubkey que envia el
device, pero verifica que el deviceID sea uno de los que vendimos. Si se detecta
fraude, se bloquea el deviceID.

Por ahora es una lista simple en archivo. En produccion: base de datos con el
registro de fabricacion (deviceID, fecha venta, cliente, estado).
"""

import json
import os
import threading

_LOCK = threading.Lock()
_PATH = os.getenv("SOLD_DEVICES_PATH",
                  os.path.join(os.path.dirname(__file__), "sold_devices.json"))


def _load() -> dict:
    if not os.path.exists(_PATH):
        return {"devices": {}}
    try:
        with open(_PATH) as f:
            return json.load(f)
    except Exception:
        return {"devices": {}}


def _save(data: dict) -> None:
    tmp = _PATH + ".tmp"
    with open(tmp, "w") as f:
        json.dump(data, f, indent=2)
    os.replace(tmp, _PATH)


def is_sold(device_id: str) -> bool:
    """True si el deviceID esta en la lista de vendidos y NO bloqueado."""
    with _LOCK:
        d = _load()["devices"].get(device_id)
        return bool(d) and not d.get("blocked", False)


def is_blocked(device_id: str) -> bool:
    with _LOCK:
        d = _load()["devices"].get(device_id)
        return bool(d) and d.get("blocked", False)


def register_sold(device_id: str, note: str = "") -> None:
    """Registra un deviceID como vendido (paso de fabrica/venta)."""
    with _LOCK:
        data = _load()
        data["devices"][device_id] = {"blocked": False, "note": note}
        _save(data)


def block(device_id: str, reason: str = "") -> None:
    """Bloquea un deviceID (fraude detectado)."""
    with _LOCK:
        data = _load()
        if device_id in data["devices"]:
            data["devices"][device_id]["blocked"] = True
            data["devices"][device_id]["block_reason"] = reason
            _save(data)


def get_pubkey(device_id: str) -> str | None:
    """Pubkey registrada para el device (si se guardo en el match), hex."""
    with _LOCK:
        d = _load()["devices"].get(device_id)
        return d.get("pubkey") if d else None


def set_pubkey(device_id: str, pubkey_hex: str) -> None:
    """Guarda la pubkey vista en el primer match (TOFU para A1)."""
    with _LOCK:
        data = _load()
        if device_id in data["devices"]:
            data["devices"][device_id]["pubkey"] = pubkey_hex
            _save(data)
