# MiniHSM v2.0

ESP32-S3 · mbedTLS PSA · PAdES · XAdES · CAdES · ATECC608B-ready

## Estructura

```
miniHSM/
├── firmware/                    ← ESP32-S3 (ESP-IDF v5.x)
│   ├── main/
│   │   ├── crypto_engine/       ← ECDSA P-256, firma DER, SHA-256
│   │   ├── vault_manager/       ← keypair en NVS cifrado
│   │   ├── cert_manager/        ← X.509 autofirmado, CSR, CA ceremony
│   │   ├── policy_engine/       ← tokens HMAC, anti-replay
│   │   ├── audit_engine/        ← log circular firmado
│   │   └── network_engine/      ← WiFi + HTTP REST API
│   ├── CMakeLists.txt
│   ├── partitions.csv
│   ├── sdkconfig.defaults
│   └── test_client/
│       └── test_client.py
└── optimizer/                   ← Python (FastAPI)
    ├── minihsm/client.py        ← cliente HTTP del firmware
    ├── signing/pades.py         ← firma PDF (PAdES-B-B)
    ├── signing/xades.py         ← firma XML / facturas SUNAT
    ├── signing/cades.py         ← firma datos genéricos
    ├── api/main.py              ← FastAPI server
    └── requirements.txt
```

## API del firmware

```
POST /sign    → digest + kuser → firma DER + certificado PEM
POST /verify  → digest + firma → valid: true|false
GET  /cert    → certificado PEM actual
GET  /csr     → CSR PKCS#10 para CA ceremony
POST /cert    → cargar certificado firmado por CA
GET  /device  → info del dispositivo
GET  /health  → uptime, ops, estado
GET  /audit   → log de operaciones firmado
GET  /token   → [DEV ONLY] generar token
```

## Setup firmware

```bash
# Requiere ESP-IDF v5.x
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbserial-* flash monitor
```

## Setup optimizer

```bash
cd optimizer
pip install -r requirements.txt
export MINIHSM_HOST=192.168.1.100
export MINIHSM_SECRET=<hex del HMAC secret>
cd api && python main.py
```

## Swap a ATECC608B

Solo reemplazar las tres funciones en `firmware/main/crypto_engine/crypto_engine.c`:
- `crypto_generate_keypair()` → genKey en slot ATECC via I2C
- `crypto_sign()`             → sign  en slot ATECC via I2C
- `crypto_verify()`           → verify via ATECC

Todo lo demás — vault, cert, policy, audit, network, optimizer — sin cambios.
