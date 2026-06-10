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
│   └── partitions.csv
└── optimizer/                   ← Python (FastAPI)
    ├── minihsm/client.py        ← cliente HTTP del firmware
    ├── signing/pades.py         ← firma PDF
    ├── signing/xades.py         ← firma XML / facturas SUNAT
    ├── signing/cades.py         ← firma datos genéricos
    └── api/main.py              ← FastAPI server
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
GET  /token   → [DEV] generar token
```

## Cambios v2.0 vs v1.0 (MiniHSMOld)

- Firma DER en lugar de raw r||s (PAdES/CAdES/XAdES compatible)
- Nuevo módulo cert_manager: cert autofirmado + CSR + CA ceremony
- /sign devuelve certificado junto a la firma
- Nuevos endpoints: /cert (GET/POST) y /csr
- Optimizer Python con soporte PAdES, XAdES y CAdES
- Partición NVS dedicada para certificados

## Swap a ATECC608B

Solo reemplazar las implementaciones en `crypto_engine.c`:
- `crypto_generate_keypair()` → genKey en slot ATECC
- `crypto_sign()` → sign en slot ATECC via I2C
- `crypto_verify()` → verify via ATECC
- Todo lo demás (vault, cert, policy, audit, network, optimizer) = sin cambios
