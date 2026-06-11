# Xami — Plan de Trabajo (Ejecución)

> El "cuándo y en qué orden". El QUÉ y POR QUÉ están en CHANGELOG_PENDIENTES.md
> y CONTEXTO_FIRMADO_BLOCKCHAIN.md — aquí solo se REFERENCIAN, no se duplican.
> Marcar [x] al completar. Fecha inicio: 2026-06-10.
>
> RE-PRIORIZADO 2026-06-11: el orden ahora prioriza lo que CAMBIA EL CONTRATO
> (entradas/salidas) de las APIs públicas — conviene estabilizar ese contrato
> antes de tener clientes integrados. Lo interno (seguridad device<->server,
> ceremonia CA) va después porque NO cambia el contrato visible al cliente.

## Reparto de tareas
- **Claude (server/manyao):** código en repo, builds CI, config servidor, pruebas de endpoints.
- **Usuario (hardware):** flasheo del dispositivo, validación en Acrobat, decisiones de
  criterio, pruebas con clientes reales.

---

## ✅ HECHO Y VALIDADO EN HARDWARE (device 00da0f3b57ec8f14 · ver CHANGELOG)

- [x] **Bloque 9 — Match**: secreto compartido device<->server (ECDH efímero + HKDF + AES-256-GCM). VALIDADO.
- [x] **Bloque 10 — Polling de firma**: el heartbeat entrega el job + token `kuser` (HMAC simple sobre el secreto del match) + `nextPollSeconds`. VALIDADO (firmware-v35).
- [x] **Firma real E2E**: el mini firma el digest, la DER verifica contra la pubkey del match, **sin doble hash**. VALIDADO.
- [x] **Bloque 8 — Forma 2**: `POST /v1/signatures/pdf` (subir PDF -> el mini firma -> descargar). Flujo async: `requestId` / `GET .../{id}` (estado) / `GET .../{id}/download`. Incluye:
  - [x] Firma invisible o **visible** (página + coordenadas/box).
  - [x] **Atributos del diccionario** independientes: `name`, `reason`, `location`, `contact` (/Name,/Reason,/Location,/ContactInfo).
  - [x] **Sello en 3 modos** (`stamp_source`): `attributes` | `default` | `custom`.
  - [x] **Imagen del sello**: `image_mode` = `background` | `left` (+ `image_opacity`).
  - [x] **Modo** `approval` | `certify` (DocMDP NO_CHANGES sella el documento).
  - [x] **TSA RFC 3161** (`tsa_url`) -> PAdES-T. VALIDADO contra freetsa.org.
  - [x] Fix PDFs con **xref híbrido** (strict=False).
  - [x] `GET /v1/signatures/ca.pem` (CA de prueba para confiar en Acrobat).
  - [x] **CA de desarrollo persistente** (dev_pki) que certifica la pubkey REAL del mini.
  - [x] CORS para xami.run. Web operativa: https://xami.run/firmar/
- [x] **Infra/bugs**: `--workers 1` (cola/registro en RAM compartidos), `/health` vía registro de heartbeats, `/openapi.json` 500 (forward-ref) resuelto.

---

## PENDIENTE — RE-PRIORIZADO POR IMPACTO EN EL CONTRATO DE API

### P1 · Cambian la SALIDA de (casi) todas las APIs → hacer PRIMERO
- [ ] **Bloque 0 — Envelope común** `{meta,data}` con `time/timeUnix/timeSynced` + NTP en firmware. Afecta la salida de TODOS los endpoints; hacerlo antes de que haya clientes que dependan del formato actual. (Claude: firmware + optimizer)
- [ ] **Bloque 1 — /health estructurado**: version/build/release, device con modelo, cert state, NTP time. (cambia salida /health)
- [ ] **Bloque 2 — /device estructurado**: deviceId con modelo, firmware estructurado, bloque de tiempo. (cambia salida /device)

### P2 · Nuevas ENTRADAS/SALIDAS de API (servicios de firma que faltan)
- [ ] **Bloque 8 — Forma 1**: `POST /v1/signatures/digest` (el cliente arma el PAdES y manda solo el hash -> el mini firma -> devuelve firma+cert). Envoltorio fino sobre la cola ya existente. (nueva API)
- [ ] **Compatibilidad UANATACA**: `/signbox/api/v1/sign` con payload homologado. (nueva API de entrada; homologación con cliente real)
- [ ] **Bloque 4 — Verification-as-a-Service**: endpoint para verificar firmas/documentos. (nueva API)

### P3 · Cambian salida / identidad (menos urgente)
- [ ] **Bloque 3 — /device como VC W3C 2.0 (COSE)**: la salida de /device pasa a credencial verificable. (cambia salida)
- [ ] **XAMI_MODEL en Kconfig**: modelo en device + SoftAP. (firmware; afecta device/SoftAP)

### P4 · NO cambian el contrato de API (internas / seguridad / confianza) → DESPUÉS
- [ ] **Ceremonia CA (Nivel B)**: cert del mini emitido por CA pública/cualificada -> verde en Acrobat sin importar nada. No cambia entrada/salida; cambia el cert embebido. (desbloquea validez legal visible)
- [ ] **Fase C — Autorización avanzada (Bloque 5+10 unificado)**: doble candado VaultStamping + KUser, **rotación de KMaster por arranque**, anti-replay por época, validación del heartbeat entrante. Interno device<->server; **toca FIRMWARE (build+flash)**; NO cambia el contrato del cliente. [HOY: solo candado simple HMAC sobre el secreto del match; sin rotación]
- [ ] **Provisioning seguro del secret** device<->server (no por red).
- [ ] **Bloque 7 — Sellado stamping.io** (attestation en respuestas de info externa).
- [ ] **Bloque 6 — Auditoría persistente y atestada**.
- [ ] **zkSNARK Groth16** (no-repudio portable) — capa posterior server-side.

---

## EN PARALELO (no bloquean)
- [ ] Borrar código viejo /home/fileserver/.../serverHSM tras validar api.xami.run
- [ ] Subir vaultStamping.js y colección UANATACA a docs/referencias/
- [ ] Recuperación de red completa (escalera + portal con contexto)
- [ ] Web /firmar: TSAs predefinidas en desplegable; preview para arrastrar el box; afinar reparto imagen/texto del modo `left`

---

## PROGRESO GLOBAL
- **Núcleo**: ✅ match (B9) + polling (B10) + firma real E2E.
- **Bloque 8 Forma 2**: ✅ COMPLETO (visible, atributos, 3 modos de sello, imagen fondo/izquierda, approval/certify, TSA/PAdES-T, CA dev, web).
- **Pendiente contrato de API (P1/P2/P3)**: Bloque 0 envelope, /health, /device, Forma 1 digest, UANATACA, verificación, VC W3C.
- **Pendiente interno (P4)**: ceremonia CA, Fase C (VaultStamping + rotación KMaster), provisioning, auditoría, zkSNARK.
