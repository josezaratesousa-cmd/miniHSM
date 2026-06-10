# MiniHSM Optimizer

Servicio Python que conecta aplicaciones de negocio con el MiniHSM ESP32-S3.

## Responsabilidades

| El Optimizer hace | El MiniHSM hace |
|---|---|
| Calcula hash del documento | Firma el hash |
| Arma la estructura PAdES/XAdES/CAdES | Devuelve firma DER + certificado |
| Embebe firma en el documento | Nunca ve el documento completo |
| Llama a la TSA para sello de tiempo | Custodia la clave privada |

## Estructura

```
optimizer/
├── minihsm/client.py     ← cliente HTTP para el firmware
├── signing/
│   ├── pades.py          ← firma PDF (PAdES-B-B / B-T)
│   ├── xades.py          ← firma XML (XAdES-BES, compatible SUNAT)
│   └── cades.py          ← firma datos genéricos (CAdES-BES)
└── api/main.py           ← FastAPI server
```

## Inicio rápido

```bash
pip install -r requirements.txt

# Configurar variables de entorno
export MINIHSM_HOST=192.168.1.100
export MINIHSM_SECRET=<hex del HMAC secret del dispositivo>

# Levantar el servidor
cd api && python main.py
```

## Endpoints

```
GET  /health          → estado del optimizer + dispositivo
GET  /device          → info del ESP32-S3
GET  /cert            → certificado PEM actual
GET  /csr             → CSR PKCS#10 para enviar a una CA
POST /cert            → cargar certificado firmado por CA
POST /sign/digest     → firmar un hash directamente
POST /sign/pdf        → firmar PDF (PAdES-B-B)
POST /sign/xml        → firmar XML (XAdES-BES / SUNAT)
POST /sign/data       → firmar datos genéricos (CAdES-BES)
```

## Ceremonia de CA

```python
from minihsm.client import MiniHSMClient

client = MiniHSMClient("192.168.1.100")

# 1. Obtener el CSR del dispositivo
csr_pem = client.get_csr_pem()

# 2. Enviar csr_pem a tu CA (interna o publica)
# ... la CA devuelve signed_cert_pem ...

# 3. Cargar el certificado firmado de vuelta al dispositivo
client.load_ca_certificate(signed_cert_pem)
# El dispositivo queda en estado PROVISIONED
```
