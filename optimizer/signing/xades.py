"""
XAdES Signer — firma XML usando el MiniHSM.

Soporta XAdES-BES (requerido por SUNAT Peru para facturas electronicas UBL 2.1).
El hash del XML canonicalizado se envia al MiniHSM; el XML nunca sale.
"""

import hashlib
import base64
import uuid
from datetime import datetime, timezone
from lxml import etree
from cryptography import x509
from cryptography.hazmat.backends import default_backend

from minihsm.client import MiniHSMClient

# Namespaces estandar XAdES / XMLDSig
NS_DS    = "http://www.w3.org/2000/09/xmldsig#"
NS_XADES = "http://uri.etsi.org/01903/v1.3.2#"
NS_MAP   = {"ds": NS_DS, "xades": NS_XADES}

# Algoritmos
C14N_ALG      = "http://www.w3.org/TR/2001/REC-xml-c14n-20010315"
SIG_ALG       = "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha256"
DIGEST_ALG    = "http://www.w3.org/2001/04/xmlenc#sha256"
TRANSFORM_ENV = "http://www.w3.org/2000/09/xmldsig#enveloped-signature"


def _c14n(element) -> bytes:
    """Canonicaliza un elemento XML (C14N exclusivo)."""
    return etree.tostring(element, method="c14n", exclusive=True)


def _sha256_b64(data: bytes) -> str:
    """SHA-256 en base64."""
    return base64.b64encode(hashlib.sha256(data).digest()).decode()


def _sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sign_xml(
    xml_content: Union[str, bytes],
    client: MiniHSMClient,
    reference_uri: str = "",
) -> bytes:
    """
    Firma un documento XML con XAdES-BES.

    Args:
        xml_content:   XML a firmar (str o bytes)
        client:        MiniHSMClient conectado al dispositivo
        reference_uri: URI de referencia (vacio = documento completo)

    Returns:
        XML firmado como bytes (UTF-8)
    """
    if isinstance(xml_content, str):
        xml_content = xml_content.encode("utf-8")

    root = etree.fromstring(xml_content)

    # Obtener certificado del dispositivo
    cert_pem  = client.get_certificate_pem()
    cert_obj  = x509.load_pem_x509_certificate(cert_pem.encode(), default_backend())
    cert_der  = cert_obj.public_bytes(
        serialization.Encoding.DER if hasattr(cert_obj, "public_bytes")
        else None
    )
    from cryptography.hazmat.primitives.serialization import Encoding
    cert_der  = cert_obj.public_bytes(Encoding.DER)
    cert_b64  = base64.b64encode(cert_der).decode()

    device_id = client.get_device_info().device_id
    sig_id    = f"Signature-{device_id}-{uuid.uuid4().hex[:8]}"
    ref_id    = f"Reference-{uuid.uuid4().hex[:8]}"
    sp_id     = f"SignedProperties-{uuid.uuid4().hex[:8]}"

    # ── 1. Calcular digest del documento (Reference) ──────────────────────────
    c14n_doc    = _c14n(root)
    doc_digest  = _sha256_b64(c14n_doc)

    # ── 2. Construir SignedProperties (XAdES) ─────────────────────────────────
    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    sp_elem = etree.Element(f"{{{NS_XADES}}}SignedProperties",
                             Id=sp_id, nsmap={"xades": NS_XADES})

    ssp = etree.SubElement(sp_elem, f"{{{NS_XADES}}}SignedSignatureProperties")
    etree.SubElement(ssp, f"{{{NS_XADES}}}SigningTime").text = now

    # Certificado en SigningCertificateV2
    sc      = etree.SubElement(ssp, f"{{{NS_XADES}}}SigningCertificateV2")
    cert_el = etree.SubElement(sc, f"{{{NS_XADES}}}Cert")
    cd      = etree.SubElement(cert_el, f"{{{NS_XADES}}}CertDigest")
    etree.SubElement(cd, f"{{{NS_DS}}}DigestMethod",
                     Algorithm=DIGEST_ALG)
    etree.SubElement(cd, f"{{{NS_DS}}}DigestValue").text = _sha256_b64(cert_der)

    # ── 3. Calcular digest de SignedProperties ────────────────────────────────
    c14n_sp    = _c14n(sp_elem)
    sp_digest  = _sha256_b64(c14n_sp)

    # ── 4. Construir SignedInfo ────────────────────────────────────────────────
    ds     = f"{{{NS_DS}}}"
    si     = etree.Element(f"{ds}SignedInfo", nsmap={"ds": NS_DS})

    cm = etree.SubElement(si, f"{ds}CanonicalizationMethod",
                           Algorithm=C14N_ALG)
    sm = etree.SubElement(si, f"{ds}SignatureMethod",
                           Algorithm=SIG_ALG)

    # Reference 1 — documento
    ref1 = etree.SubElement(si, f"{ds}Reference",
                             Id=ref_id, URI=f"#{reference_uri}" if reference_uri else "")
    tf1  = etree.SubElement(ref1, f"{ds}Transforms")
    etree.SubElement(tf1, f"{ds}Transform", Algorithm=TRANSFORM_ENV)
    etree.SubElement(tf1, f"{ds}Transform", Algorithm=C14N_ALG)
    etree.SubElement(etree.SubElement(ref1, f"{ds}DigestMethod"),
                     "dummy").getparent().set("Algorithm", DIGEST_ALG)
    ref1.remove(ref1.find(f"{ds}DigestMethod"))
    dm1 = etree.SubElement(ref1, f"{ds}DigestMethod"); dm1.set("Algorithm", DIGEST_ALG)
    etree.SubElement(ref1, f"{ds}DigestValue").text = doc_digest

    # Reference 2 — SignedProperties
    ref2 = etree.SubElement(si, f"{ds}Reference",
                             Type=f"{NS_XADES}SignedProperties",
                             URI=f"#{sp_id}")
    dm2 = etree.SubElement(ref2, f"{ds}DigestMethod"); dm2.set("Algorithm", DIGEST_ALG)
    etree.SubElement(ref2, f"{ds}DigestValue").text = sp_digest

    # ── 5. Firmar SignedInfo ──────────────────────────────────────────────────
    c14n_si   = _c14n(si)
    si_digest = _sha256_hex(c14n_si)
    sign_result = client.sign(si_digest)

    sig_b64 = base64.b64encode(
        bytes.fromhex(sign_result.signature_hex)
    ).decode()

    # ── 6. Construir elemento Signature completo ──────────────────────────────
    signature = etree.Element(f"{ds}Signature", Id=sig_id, nsmap={"ds": NS_DS, "xades": NS_XADES})
    signature.append(si)
    etree.SubElement(signature, f"{ds}SignatureValue").text = sig_b64

    # KeyInfo con el certificado
    ki  = etree.SubElement(signature, f"{ds}KeyInfo")
    x5d = etree.SubElement(etree.SubElement(ki, f"{ds}X509Data"), f"{ds}X509Certificate")
    x5d.text = cert_b64

    # Object con QualifyingProperties (XAdES)
    obj    = etree.SubElement(signature, f"{ds}Object")
    qp     = etree.SubElement(obj, f"{{{NS_XADES}}}QualifyingProperties",
                               Target=f"#{sig_id}")
    qp.append(sp_elem)

    # Insertar la firma en el documento
    root.append(signature)

    return etree.tostring(root, encoding="UTF-8", xml_declaration=True)


# Importar Union
from typing import Union
