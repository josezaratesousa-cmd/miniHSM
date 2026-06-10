"""
Criptografia del emparejamiento (match).

- verify_proof_of_possession: comprueba que una firma fue hecha por el dueno de la
  pubkey declarada (ECDSA P-256, firma DER sobre SHA-256 del challenge).
- pubkey_from_uncompressed: parsea la pubkey raw del device (formato 04||X||Y, 65 bytes).

El cifrado ECIES de los secretos se agrega en la Capa 2.
"""

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import Prehashed
from cryptography.hazmat.primitives import hashes
from cryptography.exceptions import InvalidSignature


def pubkey_from_uncompressed(pubkey_hex: str) -> ec.EllipticCurvePublicKey:
    """
    Parsea una pubkey EC P-256 en formato uncompressed (04||X||Y, 65 bytes = 130 hex).
    Es el formato que exporta el miniHSM (crypto_engine, PSA).
    """
    raw = bytes.fromhex(pubkey_hex)
    if len(raw) != 65 or raw[0] != 0x04:
        raise ValueError("pubkey debe ser uncompressed 04||X||Y (65 bytes)")
    return ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), raw)


def verify_proof_of_possession(pubkey_hex: str, challenge: bytes,
                                signature_der: bytes) -> bool:
    """
    Verifica que signature_der es una firma valida de SHA-256(challenge) hecha por
    la privada correspondiente a pubkey_hex. Demuestra posesion de la clave privada.

    challenge: los bytes que el device firmo (deviceId + nonce, p.ej.)
    signature_der: la firma en formato DER (como la produce el miniHSM)
    """
    try:
        pub = pubkey_from_uncompressed(pubkey_hex)
        # El miniHSM firma el SHA-256 del mensaje. cryptography con ECDSA(SHA256)
        # hashea internamente, asi que pasamos el challenge crudo y que el hash lo
        # haga la libreria.
        pub.verify(signature_der, challenge, ec.ECDSA(hashes.SHA256()))
        return True
    except (InvalidSignature, ValueError):
        return False


# ── ECIES (Capa 2) — cifrar secretos para el device ───────────────────────────
# Esquema: ECDH(efimero, pubkey_device) -> HKDF-SHA256 -> AES-256-GCM
# Formato de salida (todo concatenado, hex):
#   eph_pubkey(65) || iv(12) || ciphertext || tag(16)
# El device reconstruye: ECDH(su_privada, eph_pubkey) -> HKDF -> AES-GCM decrypt

import os
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives import serialization

_HKDF_INFO = b"xami-match-v1"


def ecies_encrypt(pubkey_hex: str, plaintext: bytes) -> str:
    """
    Cifra plaintext para el dueno de pubkey_hex (EC P-256).
    Devuelve hex de: eph_pub(65) || iv(12) || ct || tag(16).
    """
    pub = pubkey_from_uncompressed(pubkey_hex)

    # 1. Par efimero
    eph = ec.generate_private_key(ec.SECP256R1())
    eph_pub_raw = eph.public_key().public_bytes(
        serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)

    # 2. ECDH -> shared secret
    shared = eph.exchange(ec.ECDH(), pub)

    # 3. HKDF-SHA256 -> clave AES-256
    key = HKDF(algorithm=hashes.SHA256(), length=32, salt=b"\x00"*32,
               info=_HKDF_INFO).derive(shared)

    # 4. AES-256-GCM
    iv = os.urandom(12)
    aes = AESGCM(key)
    ct_and_tag = aes.encrypt(iv, plaintext, None)  # ct||tag (tag de 16 al final)

    return (eph_pub_raw + iv + ct_and_tag).hex()


def ecies_decrypt_with_private(priv: ec.EllipticCurvePrivateKey, blob_hex: str) -> bytes:
    """
    Solo para TESTS del lado server (el device lo hace en C).
    Descifra un blob ecies_encrypt usando la privada.
    """
    blob = bytes.fromhex(blob_hex)
    eph_pub_raw = blob[:65]
    iv = blob[65:77]
    ct_and_tag = blob[77:]
    eph_pub = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), eph_pub_raw)
    shared = priv.exchange(ec.ECDH(), eph_pub)
    key = HKDF(algorithm=hashes.SHA256(), length=32, salt=b"\x00"*32,
               info=_HKDF_INFO).derive(shared)
    return AESGCM(key).decrypt(iv, ct_and_tag, None)
