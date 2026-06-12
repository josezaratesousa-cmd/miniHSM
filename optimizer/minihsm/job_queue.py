"""
Cola de trabajos de firma por device (Bloque 10).

El server NO puede conectar hacia el device (NAT). El device hace polling via
heartbeat y recoge aqui sus trabajos pendientes. Cola en RAM, thread-safe.

Estados de un job:
  PENDING   -> encolado, esperando que el device lo recoja en su heartbeat
  DELIVERED -> entregado al device (se le acuno token), esperando resultado
  DONE      -> el device devolvio la firma
  ERROR     -> el device reporto un error

En RAM: se pierde al reiniciar el service. Suficiente para e2e y operacion
normal (un job vive segundos). Persistencia en JSON queda pendiente si hace falta.
"""

import threading
import time
import secrets as _secrets

_LOCK = threading.Lock()

# device_id -> { requestId -> job_dict }
_JOBS: dict[str, dict[str, dict]] = {}

PENDING   = "PENDING"
DELIVERED = "DELIVERED"
DONE      = "DONE"
ERROR     = "ERROR"


def _new_request_id() -> str:
    return "req_" + _secrets.token_hex(8)


def enqueue(device_id: str, digest: str, credential_id=None, auth=None) -> str:
    """Encola un trabajo de firma para un device. Devuelve el requestId.
    credential_id: slot de credencial custodiada (None = clave de iniciacion del device).
    auth: blob opaco (passphrase/TOTP cifrados para el chip), transportado sin leerse."""
    rid = _new_request_id()
    with _LOCK:
        _JOBS.setdefault(device_id, {})[rid] = {
            "requestId":    rid,
            "deviceId":     device_id,
            "digest":       digest,
            "credentialId": credential_id,
            "auth":         auth,
            "status":       PENDING,
            "result":       None,
            "createdAt":    time.time(),
        }
    return rid


def next_pending(device_id: str) -> dict | None:
    """Devuelve (sin mutar) el job PENDING mas antiguo del device, o None."""
    with _LOCK:
        jobs = _JOBS.get(device_id, {})
        pend = [j for j in jobs.values() if j["status"] == PENDING]
        if not pend:
            return None
        pend.sort(key=lambda j: j["createdAt"])
        return dict(pend[0])


def mark_delivered(device_id: str, request_id: str) -> None:
    with _LOCK:
        j = _JOBS.get(device_id, {}).get(request_id)
        if j and j["status"] == PENDING:
            j["status"] = DELIVERED


def set_result(device_id: str, request_id: str, result: dict,
               status: str = DONE) -> bool:
    """Guarda el resultado (firma+cert) y marca DONE/ERROR. True si existia."""
    with _LOCK:
        j = _JOBS.get(device_id, {}).get(request_id)
        if not j:
            return False
        j["result"] = result
        j["status"] = status
        return True


def get(device_id: str, request_id: str) -> dict | None:
    with _LOCK:
        j = _JOBS.get(device_id, {}).get(request_id)
        return dict(j) if j else None
