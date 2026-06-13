"""
Device registration & heartbeat endpoints.
El miniHSM llama aquí cada 5 minutos.
El serverHSM extrae la IP del request y la guarda.

Bloque 10: el heartbeat ES el polling. Su respuesta lleva, ademas del registro,
el trabajo de firma pendiente (si hay) y nextPollSeconds (ritmo dictado por server).
"""

import logging
import hmac
import hashlib
import secrets as _secrets

from api import credentials
from fastapi import APIRouter, Request, HTTPException
from pydantic import BaseModel

from minihsm.registry import registry, DeviceEntry
from minihsm import sold_devices, device_secrets, job_queue
from minihsm.crypto_match import verify_proof_of_possession, ecies_encrypt

log    = logging.getLogger("devices")
router = APIRouter(prefix="/devices", tags=["devices"])

# Ritmo de polling que el server dicta al device (segundos). Configurable por
# panel web mas adelante (por device o global). Por ahora constante.
NEXT_POLL_SECONDS = 25


def _mint_kuser(secret_hex: str, ts: int, nonce: str) -> str:
    """
    Acuna un token KUser que el device validara con policy_validate_token.
    Esquema EXACTO del firmware: HMAC-SHA256(secret, "minihsm:{ts}:{nonce}") hex.
    El ts DEBE ser el reloj del device (el que mando en su heartbeat), porque el
    device valida contra su esp_timer (uptime), no UTC (NTP pendiente, Bloque 0).
    """
    msg = f"minihsm:{ts}:{nonce}".encode()
    return hmac.new(bytes.fromhex(secret_hex), msg, hashlib.sha256).hexdigest()


class HeartbeatRequest(BaseModel):
    deviceId:  str
    token:     str
    timestamp: int
    nonce:     str
    firmware:  str = ""


class HeartbeatResponse(BaseModel):
    status:          str
    deviceId:        str
    ip:              str
    message:         str
    nextPollSeconds: int          = NEXT_POLL_SECONDS
    job:             dict | None  = None
    ceremony:        dict | None  = None


@router.post("/heartbeat", response_model=HeartbeatResponse)
def heartbeat(req: Request, body: HeartbeatRequest):
    """
    Recibe el heartbeat del miniHSM (= su polling).
    Extrae la IP publica del request, registra, y de paso le entrega el trabajo
    de firma pendiente (si hay) con un token recien acunado, y el ritmo de poll.
    """
    ip = (
        req.headers.get("X-Forwarded-For", "").split(",")[0].strip()
        or req.headers.get("X-Real-IP", "")
        or req.client.host
    )
    if not ip:
        raise HTTPException(400, "Cannot determine client IP")

    # TODO: validar token HMAC del heartbeat con el secret del device
    registry.register(device_id=body.deviceId, ip=ip, firmware=body.firmware)
    log.info(f"Heartbeat from {body.deviceId} @ {ip} (firmware={body.firmware})")

    # Bloque 10: hay trabajo pendiente para este device?
    job_payload = None
    pending = job_queue.next_pending(body.deviceId)
    if pending:
        secret = device_secrets.get_secret(body.deviceId)
        if not secret:
            log.warning(f"Job {pending['requestId']} pero device {body.deviceId} "
                        f"sin secret (no emparejado). No se entrega.")
        else:
            # Token fresco usando el reloj que el device acaba de reportar.
            ts    = body.timestamp
            nonce = _secrets.token_hex(8)
            kuser = _mint_kuser(secret, ts, nonce)
            job_queue.mark_delivered(body.deviceId, pending["requestId"])
            job_payload = {
                "requestId": pending["requestId"],
                "digest":    pending["digest"],
                "kuser":     kuser,
                "ts":        ts,
                "nonce":     nonce,
            }
            # Fase 2: credencial custodiada (slot) + auth opaco (passphrase/TOTP cifrados
            # para el chip). Solo se incluyen si el job los trae; ausencia = clave del device.
            if pending.get("credentialId") is not None:
                job_payload["credentialId"] = pending["credentialId"]
            if pending.get("auth") is not None:
                job_payload["auth"] = pending["auth"]
            if pending.get("sigType"):
                job_payload["sigType"] = pending["sigType"]
            log.info(f"Heartbeat entrega job {pending['requestId']} a {body.deviceId}")

    # Fase 4a: ceremonia de custodia pendiente? (el chip entra en modo AP con el secreto)
    ceremony_payload = None
    cer = credentials.pop_pending_ceremony(body.deviceId)
    if cer:
        ceremony_payload = {"ceremonyId": cer["ceremonyId"], "alias": cer["alias"],
                            "secret": cer["secret"]}
        log.info(f"Heartbeat entrega ceremonia {cer['ceremonyId']} a {body.deviceId}")

    return HeartbeatResponse(
        status          = "ok",
        deviceId        = body.deviceId,
        ip              = ip,
        message         = "registered",
        nextPollSeconds = NEXT_POLL_SECONDS,
        job             = job_payload,
        ceremony        = ceremony_payload,
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
    secretsEncrypted: str   # blob ECIES (hex): HMAC secret cifrado con la pubkey


@router.post("/match", response_model=MatchResponse)
def match(req: Request, body: MatchRequest):
    """
    Emparejamiento de un Xami con el server (Bloque 9).
    Verifica: no-bloqueado + proof of possession. Matricula TOFU. Entrega secret
    cifrado (ECIES).
    """
    if sold_devices.is_blocked(body.deviceId):
        raise HTTPException(403, f"Device {body.deviceId} is blocked")

    if not sold_devices.is_sold(body.deviceId):
        sold_devices.register_sold(body.deviceId, note="auto-matriculado en primer match")
        log.info(f"Match: {body.deviceId} MATRICULADO automaticamente (primer contacto)")

    challenge = f"{body.deviceId}:{body.timestamp}:{body.nonce}".encode()
    try:
        sig_der = bytes.fromhex(body.signature)
    except ValueError:
        raise HTTPException(400, "signature must be hex")

    if not verify_proof_of_possession(body.pubkey, challenge, sig_der):
        raise HTTPException(401, "Invalid proof of possession")

    known = sold_devices.get_pubkey(body.deviceId)
    if known is None:
        sold_devices.set_pubkey(body.deviceId, body.pubkey)
        log.info(f"Match: {body.deviceId} pubkey registrada (TOFU)")
    elif known != body.pubkey:
        raise HTTPException(409, "pubkey mismatch — posible suplantacion")

    ip = (req.headers.get("X-Forwarded-For", "").split(",")[0].strip()
          or req.client.host)
    registry.register(device_id=body.deviceId, ip=ip)

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


# ── Cola de trabajos de firma — Bloque 10 ─────────────────────────────────────

class EnqueueJobRequest(BaseModel):
    digest: str            # SHA-256 a firmar, 64 hex


class JobResultRequest(BaseModel):
    signature: str = ""    # firma DER hex (ECDSA) o raw (RSA PKCS#1v15)
    cert:      str = ""    # certificado (PEM/DER segun device)
    algorithm: str = ""    # ECDSA-P256-SHA256-DER | RSA-PKCS1v15-SHA256 (multi-cert)
    status:    str = "DONE"
    error:     str = ""


@router.post("/{device_id}/jobs")
def enqueue_job(device_id: str, body: EnqueueJobRequest):
    """Encola un trabajo de firma para el device (lo llama la web al subir doc).
       El device lo recogera en su proximo heartbeat."""
    digest = body.digest.strip().lower()
    if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
        raise HTTPException(400, "digest must be 64 hex chars (SHA-256)")
    rid = job_queue.enqueue(device_id, digest)
    log.info(f"Job encolado {rid} para {device_id}")
    return {"status": "queued", "deviceId": device_id, "requestId": rid}


@router.post("/{device_id}/jobs/{request_id}/result")
def post_job_result(device_id: str, request_id: str, body: JobResultRequest):
    """El device postea el resultado de la firma."""
    status = job_queue.DONE if body.status == "DONE" else job_queue.ERROR
    result = {"signature": body.signature, "cert": body.cert,
              "algorithm": body.algorithm, "error": body.error}
    ok = job_queue.set_result(device_id, request_id, result, status=status)
    if not ok:
        raise HTTPException(404, f"Job {request_id} not found for {device_id}")
    log.info(f"Job {request_id} de {device_id} -> {status}")
    return {"status": "ok", "requestId": request_id, "jobStatus": status}


@router.get("/{device_id}/jobs/{request_id}")
def get_job(device_id: str, request_id: str):
    """La web consulta el estado/resultado de un trabajo."""
    j = job_queue.get(device_id, request_id)
    if not j:
        raise HTTPException(404, f"Job {request_id} not found for {device_id}")
    return j
