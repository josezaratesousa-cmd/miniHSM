"""CA de desarrollo PERSISTENTE para firmas PAdES.

La CA (clave+cert) se genera una sola vez y se guarda en disco (fuera del repo).
El cert del device se emite al vuelo desde la CA + la pubkey REAL del device
(la registrada en el match, en sold_devices.json).

NO es una CA de produccion. Sirve para validar el flujo end-to-end y para que
el usuario confie la CA en Acrobat y vea el check verde. Para verde sin importar
nada, el cert del device debe emitirlo una CA publica/cualificada (ceremonia CA).
"""
import json, datetime
from pathlib import Path
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from asn1crypto import x509 as a1x509

PKI_DIR = Path("/home/xami/public_html/api/pki")
CA_KEY  = PKI_DIR / "dev_ca.key"
CA_CRT  = PKI_DIR / "dev_ca.crt"
_NB = datetime.datetime(2024, 1, 1)
_NA = datetime.datetime(2034, 1, 1)
_SOLD = Path(__file__).resolve().parent.parent / "minihsm" / "sold_devices.json"


def _ca_name():
    return x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, "Xami Dev CA"),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME, "Xami"),
    ])


def get_ca():
    """Devuelve (ca_key, ca_cert). Genera y persiste si no existen."""
    PKI_DIR.mkdir(parents=True, exist_ok=True)
    if CA_KEY.exists() and CA_CRT.exists():
        key = serialization.load_pem_private_key(CA_KEY.read_bytes(), None)
        crt = x509.load_pem_x509_certificate(CA_CRT.read_bytes())
        return key, crt
    key = ec.generate_private_key(ec.SECP256R1())
    name = _ca_name()
    crt = (x509.CertificateBuilder().subject_name(name).issuer_name(name)
           .public_key(key.public_key()).serial_number(x509.random_serial_number())
           .not_valid_before(_NB).not_valid_after(_NA)
           .add_extension(x509.BasicConstraints(ca=True, path_length=None), True)
           .sign(key, hashes.SHA256()))
    CA_KEY.write_bytes(key.private_bytes(serialization.Encoding.PEM,
                       serialization.PrivateFormat.PKCS8,
                       serialization.NoEncryption()))
    CA_CRT.write_bytes(crt.public_bytes(serialization.Encoding.PEM))
    CA_KEY.chmod(0o600)
    return key, crt


def _device_pubkey(device_id: str):
    sd = json.loads(_SOLD.read_text())
    entry = sd["devices"][device_id]
    return ec.EllipticCurvePublicKey.from_encoded_point(
        ec.SECP256R1(), bytes.fromhex(entry["pubkey"]))


def issue_device_cert(device_id: str):
    """Emite un cert X.509 para el device (pubkey real), firmado por la CA dev."""
    ca_key, ca_crt = get_ca()
    pub = _device_pubkey(device_id)
    name = x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, f"MiniHSM-{device_id}"),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME, "MiniHSM"),
        x509.NameAttribute(NameOID.COUNTRY_NAME, "PE"),
    ])
    return (x509.CertificateBuilder().subject_name(name).issuer_name(ca_crt.subject)
            .public_key(pub).serial_number(x509.random_serial_number())
            .not_valid_before(_NB).not_valid_after(_NA)
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), True)
            .add_extension(x509.KeyUsage(True, True, False, False, False,
                                         False, False, False, False), True)
            .sign(ca_key, hashes.SHA256()))


def ca_cert_pem() -> bytes:
    _, crt = get_ca()
    return crt.public_bytes(serialization.Encoding.PEM)


def device_cert_a1(device_id: str):
    der = issue_device_cert(device_id).public_bytes(serialization.Encoding.DER)
    return a1x509.Certificate.load(der)


def ca_cert_a1():
    _, crt = get_ca()
    return a1x509.Certificate.load(crt.public_bytes(serialization.Encoding.DER))
