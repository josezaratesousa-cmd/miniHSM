"""
Custodia de las llaves R del agente automatizado.

El chip, al enrolar una credencial como 'agente', genera una R aleatoria, cifra la
privada con K = KDF(R, Kmaster_chip, fingerprint) y entrega R al server (heartbeat).
El server la custodia aqui. Al encolar una firma de esa credencial, el server entrega
{R, nonce} cifrado hacia la pubkey del chip.

Seguridad: R por si sola NO descifra nada (hace falta Kmaster, que nunca sale del chip),
por eso custodiarla aqui no compromete la privada. Mejora futura: cifrado en reposo.
Indexado por device_id + fingerprint. Estado 'aceptada' = custodiada. nonce = contador
monotono anti-replay.
"""
import json
import os
import threading

_LOCK = threading.Lock()
_PATH = os.getenv("AGENT_KEYS_PATH",
                  os.path.join(os.path.dirname(__file__), "agent_keys.json"))


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


def accept(device_id: str, fingerprint: str, r_hex: str) -> None:
    """Custodia R entregada por el chip (idempotente). Estado -> aceptada. Conserva el nonce."""
    fp = fingerprint.lower()
    with _LOCK:
        d = _load()
        dev = d.setdefault(device_id, {})
        nonce = dev.get(fp, {}).get("nonce", 0)
        dev[fp] = {"R": r_hex, "state": "aceptada", "nonce": nonce}
        _save(d)


def get(device_id: str, fingerprint: str) -> dict | None:
    """Devuelve {R, state, nonce} o None."""
    with _LOCK:
        dev = _load().get(device_id) or {}
        return dev.get(fingerprint.lower())


def has_key(device_id: str, fingerprint: str) -> bool:
    """True si R esta custodiada (se puede encolar firmas de esa credencial)."""
    k = get(device_id, fingerprint)
    return bool(k and k.get("state") == "aceptada" and k.get("R"))


def next_nonce(device_id: str, fingerprint: str) -> int:
    """Incrementa y devuelve el nonce monotono (anti-replay) de esa credencial."""
    fp = fingerprint.lower()
    with _LOCK:
        d = _load()
        dev = d.setdefault(device_id, {})
        k = dev.get(fp)
        if not k:
            raise KeyError(f"sin R para {device_id}/{fp}")
        k["nonce"] = int(k.get("nonce", 0)) + 1
        dev[fp] = k
        _save(d)
        return k["nonce"]


def forget(device_id: str, fingerprint: str) -> None:
    """Olvida R (desactiva el agente de esa credencial)."""
    fp = fingerprint.lower()
    with _LOCK:
        d = _load()
        dev = d.get(device_id) or {}
        if fp in dev:
            del dev[fp]
            d[device_id] = dev
            _save(d)


def list_for(device_id: str) -> dict:
    """Estado de las llaves de un device, SIN exponer R."""
    with _LOCK:
        dev = _load().get(device_id) or {}
        return {fp: {"state": v.get("state"), "nonce": v.get("nonce", 0)}
                for fp, v in dev.items()}
