"""
CAdES Signer — firma datos arbitrarios con CAdES-BES via MiniHSM.

CAdES es firma binaria envuelta en CMS (PKCS#7).
Util para: JSON, binarios, archivos no-PDF/no-XML.
"""

import hashlib
from typing import Union
from cryptography import x509
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.serialization import Encoding
from cryptography.hazmat.primitives.asymmetric.padding import PKCS1v15
from cryptography import x509
from cryptography.x509.oid import NameOID
import datetime

from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
from cryptography.hazmat.primitives.asymmetric import utils as asym_utils

from minihsm.client import MiniHSMClient

try:
    from asn1crypto import cms, algos, core, pem as asn1pem, x509 as asn1x509
    HAS_ASN1CRYPTO = True
except ImportError:
    HAS_ASN1CRYPTO = False


def sign_data(
    data: Union[bytes, str],
    client: MiniHSMClient,
    detached: bool = True,
) -> bytes:
    """
    Firma datos con CAdES-BES.

    Args:
        data:     Datos a firmar
        client:   MiniHSMClient
        detached: True = firma detachada (solo el contenedor CMS)
                  False = firma embebida (datos + firma en el mismo CMS)

    Returns:
        Bytes del contenedor CMS/PKCS#7 (DER)
    """
    if isinstance(data, str):
        data = data.encode("utf-8")

    if not HAS_ASN1CRYPTO:
        raise ImportError(
            "asn1crypto is required for CAdES: pip install asn1crypto"
        )

    # Obtener certificado
    cert_pem  = client.get_certificate_pem()
    cert_obj  = x509.load_pem_x509_certificate(cert_pem.encode(), default_backend())
    cert_der  = cert_obj.public_bytes(Encoding.DER)

    # Hash del contenido
    digest_hex = hashlib.sha256(data).hexdigest()

    # Firmar con MiniHSM
    sign_result = client.sign(digest_hex)
    signature   = bytes.fromhex(sign_result.signature_hex)

    # Construir CMS SignedData con asn1crypto
    asn1_cert  = asn1x509.Certificate.load(cert_der)
    issuer     = asn1_cert.issuer
    serial     = asn1_cert.serial_number

    signed_data = cms.SignedData({
        "version": "v1",
        "digest_algorithms": [{"algorithm": "sha256"}],
        "encap_content_info": {
            "content_type": "data",
            "content":      None if detached else core.OctetString(data),
        },
        "certificates": [asn1_cert],
        "signer_infos": [{
            "version":              "v1",
            "sid": cms.SignerIdentifier({
                "issuer_and_serial_number": {
                    "issuer":        issuer,
                    "serial_number": serial,
                }
            }),
            "digest_algorithm":    {"algorithm": "sha256"},
            "signature_algorithm": {
                "algorithm": "1.2.840.10045.4.3.2"  # ecdsa-with-SHA256
            },
            "signature": signature,
        }],
    })

    content_info = cms.ContentInfo({
        "content_type": "signed_data",
        "content":      signed_data,
    })

    return content_info.dump()


def verify_cades(cms_bytes: bytes, data: bytes | None = None) -> dict:
    """
    Verifica un CMS/CAdES.
    Si es detached, proveer los datos originales en `data`.
    """
    if not HAS_ASN1CRYPTO:
        raise ImportError("asn1crypto required: pip install asn1crypto")

    ci     = cms.ContentInfo.load(cms_bytes)
    sd     = ci["content"]
    result = {"valid": False, "signer": None, "error": None}

    try:
        for si in sd["signer_infos"]:
            cert_der = sd["certificates"][0].dump()
            cert_obj = x509.load_der_x509_certificate(cert_der, default_backend())
            pub_key  = cert_obj.public_key()

            content = (
                data if data is not None
                else sd["encap_content_info"]["content"].native
            )
            digest   = hashlib.sha256(content).digest()
            sig_bytes = si["signature"].native

            pub_key.verify(sig_bytes, digest, ec.ECDSA(asym_utils.Prehashed()))
            result["valid"]  = True
            result["signer"] = cert_obj.subject.rfc4514_string()
    except Exception as e:
        result["error"] = str(e)

    return result
