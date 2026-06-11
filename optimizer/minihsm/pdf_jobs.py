"""Estado en RAM de trabajos de firma de PDF (Bloque 8 Forma 2).

Distinto de job_queue (que es la cola device<->server de digests). Aqui se
guarda el PDF firmado resultante mientras el cliente hace polling para
descargarlo. En RAM: se pierde al reiniciar (un trabajo vive ~segundos).
"""
import threading
import time
import secrets

_LOCK = threading.Lock()
_TASKS: dict[str, dict] = {}

PROCESSING = "processing"
DONE       = "done"
ERROR      = "error"


def create(filename: str) -> str:
    pid = "sig_" + secrets.token_hex(8)
    with _LOCK:
        _TASKS[pid] = {
            "requestId":  pid,
            "status":     PROCESSING,
            "filename":   filename,
            "signed_pdf": None,
            "error":      None,
            "createdAt":  time.time(),
        }
    return pid


def set_done(pid: str, signed_pdf: bytes) -> None:
    with _LOCK:
        t = _TASKS.get(pid)
        if t:
            t["status"] = DONE
            t["signed_pdf"] = signed_pdf


def set_error(pid: str, error) -> None:
    with _LOCK:
        t = _TASKS.get(pid)
        if t:
            t["status"] = ERROR
            t["error"] = str(error)


def get(pid: str) -> dict | None:
    with _LOCK:
        t = _TASKS.get(pid)
        return dict(t) if t else None
