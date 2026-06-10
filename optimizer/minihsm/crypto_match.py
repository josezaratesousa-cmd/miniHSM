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
