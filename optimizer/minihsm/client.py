"""
MiniHSM Client — habla con el firmware del ESP32-S3 via HTTP.
El Optimizer usa este cliente para todas las operaciones criptograficas.
"""

import time
import hmac
import hashlib
import uuid
import requests
from dataclasses import dataclass


@dataclass
class SignResult:
    signature_hex: str      # Firma ECDSA DER en hex (lista para PAdES/CAdES/XAdES)
    certificate_pem: str    # Certificado X.509 del dispositivo en PEM
    device_id: str
    request_id: str
    cert_state: str         # PROVISIONED | UNPROVISIONED


@dataclass
class DeviceInfo:
    device_id: str
    firmware: str
    pubkey_hex: str
    backend: str
    curve: str
    sig_format: str
    cert_fingerprint: str
    cert_state: str


class MiniHSMClient:
    """
    Cliente HTTP para el MiniHSM.
    Maneja autenticacion HMAC (KUser), timeouts y reintentos.
    """

    def __init__(self, host: str, port: int = 80, timeout: int = 10):
        self.base_url = f"http://{host}:{port}"
        self.timeout  = timeout
        self._secret: bytes | None = None

    # ── Autenticacion ─────────────────────────────────────────────────────────

    def set_secret(self, secret_hex: str) -> None:
        """Configura el HMAC secret (obtenido del dispositivo en primer boot)."""
        self._secret = bytes.fromhex(secret_hex)

    def _generate_token(self) -> tuple[str, int, str]:
        """Genera un KUser token valido para una sola operacion."""
        if self._secret is None:
            raise RuntimeError("Secret not set. Call set_secret() first.")
        ts    = int(time.time())
        nonce = uuid.uuid4().hex[:16]
        msg   = f"minihsm:{ts}:{nonce}".encode()
        token = hmac.new(self._secret, msg, hashlib.sha256).hexdigest()
        return token, ts, nonce

    # ── Operaciones principales ───────────────────────────────────────────────

    def sign(self, digest_hex: str, request_id: str | None = None) -> SignResult:
        """
        Firma un digest SHA-256 (32 bytes en hex).
        Devuelve la firma DER + el certificado del dispositivo.

        digest_hex: SHA-256 del documento a firmar, en hex (64 chars).
        """
        if len(digest_hex) != 64:
            raise ValueError(f"digest_hex must be 64 hex chars, got {len(digest_hex)}")

        token, ts, nonce = self._generate_token()
        req_id = request_id or f"sign-{uuid.uuid4().hex[:8]}"

        payload = {
            "requestId": req_id,
            "timestamp": ts,
            "nonce":     nonce,
            "digest":    digest_hex,
            "kuser":     token,
        }

        resp = requests.post(
            f"{self.base_url}/sign",
            json=payload,
            timeout=self.timeout
        )
        resp.raise_for_status()
        data = resp.json()

        if data.get("status") != "success":
            raise RuntimeError(f"Sign failed: {data}")

        return SignResult(
            signature_hex   = data["signature"],
            certificate_pem = data["certificate"],
            device_id       = data["deviceId"],
            request_id      = data["requestId"],
            cert_state      = data.get("certState", "UNKNOWN"),
        )

    def verify(self, digest_hex: str, signature_hex: str,
               request_id: str | None = None) -> bool:
        """Verifica una firma DER contra un digest SHA-256."""
        req_id = request_id or f"verify-{uuid.uuid4().hex[:8]}"
        resp = requests.post(
            f"{self.base_url}/verify",
            json={"requestId": req_id, "digest": digest_hex, "signature": signature_hex},
            timeout=self.timeout
        )
        resp.raise_for_status()
        return resp.json().get("valid", False)

    def get_device_info(self) -> DeviceInfo:
        """Obtiene informacion del dispositivo."""
        resp = requests.get(f"{self.base_url}/device", timeout=self.timeout)
        resp.raise_for_status()
        d = resp.json()
        return DeviceInfo(
            device_id       = d["deviceId"],
            firmware        = d["firmware"],
            pubkey_hex      = d["pubkey"],
            backend         = d["backend"],
            curve           = d["curve"],
            sig_format      = d.get("sigFormat", "DER"),
            cert_fingerprint= d.get("certFingerprint", ""),
            cert_state      = d.get("certState", "UNKNOWN"),
        )

    def get_certificate_pem(self) -> str:
        """Obtiene el certificado PEM actual del dispositivo."""
        resp = requests.get(f"{self.base_url}/cert", timeout=self.timeout)
        resp.raise_for_status()
        return resp.json()["certificate"]

    def get_csr_pem(self) -> str:
        """Obtiene el CSR PKCS#10 para enviarlo a una CA."""
        resp = requests.get(f"{self.base_url}/csr", timeout=self.timeout)
        resp.raise_for_status()
        return resp.json()["csr"]

    def load_ca_certificate(self, cert_pem: str) -> bool:
        """
        Carga un certificado firmado por una CA en el dispositivo.
        Llama a esto despues de la ceremonia de firma con la CA.
        """
        resp = requests.post(
            f"{self.base_url}/cert",
            json={"certificate": cert_pem},
            timeout=self.timeout
        )
        resp.raise_for_status()
        return resp.json().get("state") == "PROVISIONED"

    def health(self) -> dict:
        """Health check del dispositivo."""
        resp = requests.get(f"{self.base_url}/health", timeout=self.timeout)
        resp.raise_for_status()
        return resp.json()

    def get_audit_log(self) -> dict:
        """Obtiene el log de auditoría del dispositivo."""
        resp = requests.get(f"{self.base_url}/audit", timeout=self.timeout)
        resp.raise_for_status()
        return resp.json()

    # ── Solo desarrollo ───────────────────────────────────────────────────────

    def dev_get_token(self) -> tuple[str, int, str]:
        """
        [SOLO DESARROLLO] Obtiene un token desde el endpoint /token.
        En produccion el secret debe configurarse via set_secret().
        """
        resp = requests.get(f"{self.base_url}/token", timeout=self.timeout)
        resp.raise_for_status()
        d = resp.json()
        return d["token"], d["timestamp"], d["nonce"]
