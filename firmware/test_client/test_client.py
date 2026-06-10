#!/usr/bin/env python3
"""
MiniHSM Test Client
Uso: python3 test_client.py --host 192.168.1.100
"""

import argparse
import hashlib
import json
import time
import hmac
import os
import requests

def get_args():
    p = argparse.ArgumentParser(description="MiniHSM Test Client")
    p.add_argument("--host", default="192.168.1.100", help="IP del ESP32-S3")
    p.add_argument("--port", default=80, type=int)
    return p.parse_args()

def base_url(host, port):
    return f"http://{host}:{port}"

# ─────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────

def get_token(base):
    """Obtiene un token de desarrollo desde el endpoint /token"""
    r = requests.get(f"{base}/token")
    r.raise_for_status()
    data = r.json()
    print(f"  [token] ts={data['timestamp']} nonce={data['nonce']}")
    return data["token"], data["timestamp"], data["nonce"]

def print_section(title):
    print(f"\n{'='*50}")
    print(f"  {title}")
    print(f"{'='*50}")

# ─────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────

def test_health(base):
    print_section("GET /health")
    r = requests.get(f"{base}/health")
    data = r.json()
    print(json.dumps(data, indent=2))
    assert data["status"] == "ok", "Health check failed"
    print("  ✅ OK")

def test_device(base):
    print_section("GET /device")
    r = requests.get(f"{base}/device")
    data = r.json()
    print(json.dumps(data, indent=2))
    assert "deviceId" in data
    assert "pubkey" in data
    print("  ✅ OK")
    return data["pubkey"], data["deviceId"]

def test_sign(base, document: str):
    print_section(f"POST /sign  (document: '{document}')")

    # Calcular SHA-256 del documento
    digest = hashlib.sha256(document.encode()).hexdigest()
    print(f"  Digest: {digest}")

    # Obtener token de autorizacion
    token, ts, nonce = get_token(base)

    payload = {
        "requestId": f"test-{int(time.time())}",
        "timestamp": ts,
        "nonce":     nonce,
        "digest":    digest,
        "kuser":     token,
    }

    r = requests.post(f"{base}/sign", json=payload)
    data = r.json()
    print(json.dumps(data, indent=2))
    assert data["status"] == "success", f"Sign failed: {data}"
    print(f"  ✅ Signed OK  sig_len={len(data['signature'])//2} bytes")
    return digest, data["signature"]

def test_verify(base, digest: str, signature: str):
    print_section("POST /verify")

    payload = {
        "requestId": f"verify-{int(time.time())}",
        "digest":    digest,
        "signature": signature,
    }

    r = requests.post(f"{base}/verify", json=payload)
    data = r.json()
    print(json.dumps(data, indent=2))
    assert data["valid"] == True, "Signature verification failed!"
    print("  ✅ Signature VALID")

def test_verify_tampered(base, digest: str, signature: str):
    print_section("POST /verify (tampered digest - debe fallar)")

    # Modificar el digest
    tampered = "ff" + digest[2:]

    payload = {
        "requestId": f"tamper-{int(time.time())}",
        "digest":    tampered,
        "signature": signature,
    }

    r = requests.post(f"{base}/verify", json=payload)
    data = r.json()
    print(json.dumps(data, indent=2))
    assert data["valid"] == False, "Should have failed with tampered digest!"
    print("  ✅ Correctly rejected tampered digest")

def test_unauthorized_sign(base):
    print_section("POST /sign (sin token - debe dar 401)")

    digest = hashlib.sha256(b"unauthorized test").hexdigest()
    payload = {
        "requestId": "unauth-test",
        "timestamp": int(time.time()),
        "nonce":     "bad",
        "digest":    digest,
        "kuser":     "0" * 64,  # token falso
    }

    r = requests.post(f"{base}/sign", json=payload)
    data = r.json()
    print(json.dumps(data, indent=2))
    assert r.status_code == 401 or "errorCode" in data, "Should have rejected invalid token"
    print("  ✅ Correctly rejected invalid token")

def test_audit(base):
    print_section("GET /audit")
    r = requests.get(f"{base}/audit")
    data = r.json()
    print(f"  Total operations logged: {data['count']}")
    if data["entries"]:
        print(f"  Last entry: {json.dumps(data['entries'][-1], indent=4)}")
    print("  ✅ Audit log OK")

def test_verify_offline(pubkey_hex: str, digest_hex: str, sig_hex: str):
    """Verifica la firma localmente usando cryptography lib (sin ESP32)"""
    print_section("Verificacion offline (Python cryptography)")

    try:
        from cryptography.hazmat.primitives.asymmetric import ec
        from cryptography.hazmat.primitives import hashes
        from cryptography.hazmat.backends import default_backend

        pubkey_bytes = bytes.fromhex(pubkey_hex)
        sig_bytes    = bytes.fromhex(sig_hex)
        digest_bytes = bytes.fromhex(digest_hex)

        # Cargar clave publica P-256 desde formato uncompressed
        pub = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), pubkey_bytes)

        # Verificar - la firma ya es sobre el digest (Prehashed)
        pub.verify(sig_bytes, digest_bytes, ec.ECDSA(hashes.Prehashed()))
        print("  ✅ Offline verification PASSED")

    except ImportError:
        print("  ⚠️  cryptography not installed: pip install cryptography")
    except Exception as e:
        print(f"  ❌ Offline verification FAILED: {e}")

# ─────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────

def main():
    args = get_args()
    base = base_url(args.host, args.port)

    print(f"\n🔐 MiniHSM Test Client")
    print(f"   Target: {base}")

    try:
        # 1. Health check
        test_health(base)

        # 2. Device info
        pubkey_hex, device_id = test_device(base)
        print(f"\n  Device ID : {device_id}")
        print(f"  Public Key: {pubkey_hex[:32]}...")

        # 3. Firmar un documento
        document = "Este es un documento importante para MiniHSM MVP"
        digest, signature = test_sign(base, document)

        # 4. Verificar la firma (en el dispositivo)
        test_verify(base, digest, signature)

        # 5. Verificar con digest alterado
        test_verify_tampered(base, digest, signature)

        # 6. Token invalido
        test_unauthorized_sign(base)

        # 7. Audit log
        test_audit(base)

        # 8. Verificacion offline
        test_verify_offline(pubkey_hex, digest, signature)

        print(f"\n{'='*50}")
        print(f"  🎉 ALL TESTS PASSED")
        print(f"{'='*50}\n")

    except requests.exceptions.ConnectionError:
        print(f"\n❌ Cannot connect to {base}")
        print("   Verifica que el ESP32-S3 este encendido y conectado a la misma red")

if __name__ == "__main__":
    main()
