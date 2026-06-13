# Xami — Plan de Trabajo (Ejecución)

> El "cuándo y en qué orden". El QUÉ y POR QUÉ están en CHANGELOG_PENDIENTES.md
> y CONTEXTO_FIRMADO_BLOCKCHAIN.md — aquí solo se REFERENCIAN, no se duplican.
> Marcar [x] al completar. Fecha inicio: 2026-06-10.
>
> Marcadores: [x]=validado en HARDWARE · [c]=codigo listo, PENDIENTE build/flasheo ·
> [~]=parcial (codigo incompleto respecto a la tarea) · [ ]=pendiente.
> Actualizado 2026-06-12 tras la tanda RSA+multi-cert+VC (sin push aun).
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

## AUDITORIA 2026-06-13 — HALLAZGOS (codigo vs plan)

> Cumplimiento real verificado en codigo (server + firmware). Lo [x] (hardware) esta bien
> respaldado; lo marcado [c]/[~] esta en el CHIP, pero el plan mide el CONTRATO PUBLICO (server).

- **H1 — B1/B2/B3 son firmware-only:** el contrato PUBLICO (server main.py) sigue con el /health
  viejo (registry-based) y /device proxy (get_client()). Falta el lado server + como exponer la VC
  del chip con NAT (p. ej. que el heartbeat la empuje).
- **H2 — R3 bug RSA:** pades_polling.async_sign_raw(dry_run) devuelve bytes(72) fijo (tam. ECDSA);
  con RSA-2048 (256B) el /Contents reservado se queda corto -> PAdES RSA roto. [RESUELTO por S1]
- **H3 — credentialId YA se envia** (Forma 2 -> job_queue.enqueue -> job del heartbeat). Lo nuevo
  (sigType) no se manda aun, pero el chip lo trata como opcional. (Corrige una nota previa.)
- **H4 — Endpoints legacy en main.py** (/sign/*, /device, /cert, /csr via get_client directo):
  no alcanzan al device con NAT/polling. Cruft pre-polling a revisar/borrar.
- **H5 — Punto ciego del plan:** custodia .p12 + RSA + multi-cert no figura aqui (vive en
  PLAN_CUSTODIA_FASES.md) aunque ya tiene piezas en el server (Forma 2 con credential_id/auth).

## PENDIENTES SIMPLES PRIORIZADAS (server/web, sin build/flasheo) — sprint actual
- [x] **S1 — Fix dry_run RSA (pades_polling.py)** HECHO 2026-06-13: helper `_sig_size_for_cert`
      (RSA=modulus/8 -> 256; EC P-256 -> 72) en el dry_run. DESPLEGADO y activo (servicio reiniciado).
      Helper validado con certs RSA/EC reales. Falta E2E con un chip RSA flasheado.
- [x] **S2 — Desplegable de TSAs en /firmar** YA ESTABA: `#tsasel` (FreeTSA/DigiCert/Sectigo/Certum/
      Personalizada) + JS que rellena tsa_url. (Hallazgo de auditoria: el item "EN PARALELO" de la web
      ya tenia esto; faltan las otras 2 sub-tareas: preview para arrastrar el box y reparto img/texto left.)
- [x] **S3 — /health del server estructurado** HECHO 2026-06-13: + version (2.0.0), release (git short),
      time/timeUnix, ademas de los conteos/devices. DESPLEGADO. (seed del envelope B0 lado server)
- [x] **S4 — Forma 1 /v1/signatures/digest** HECHO 2026-06-13: POST {digest, device_id?, credential_id?,
      auth?} -> encola en la cola, espera y devuelve {signature, cert, algorithm}. DESPLEGADO (400/503/openapi
      verificados; E2E pendiente de device online). BONUS: el resultado del job ahora captura "algorithm"
      (JobResultRequest), que antes se descartaba -> tambien beneficia Forma 2/multi-cert.
- NO-de-un-turno (para no confundir): B2/B3 publico (NAT), XAMI_MODEL completo (firmware+flasheo),
  B4 verificacion, B0 envelope (transversal a todos los endpoints), y todo P4.

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
- [~] **Bloque 0 — Envelope común** `{meta,data}` con `time/timeUnix/timeSynced` + NTP en firmware. Afecta la salida de TODOS los endpoints. (Claude: firmware + optimizer)
      - AVANCE: bloque de tiempo `time` (ISO8601) + `timeSynced` ya en `/health` y `/device` (helper `att_add_time`, NTP ya existia).
      - FALTA: envelope `{meta,data}`, `timeUnix`, y el lado optimizer (server).
- [c] **Bloque 1 — /health estructurado**: status, device `Xami-A1-<id>`, model, firmware`{version,build,release}`, cert(self/ca-signed), uptime, opCount, time, timeSynced, secureBoot. (cambia salida /health)
- [c] **Bloque 2 — /device estructurado**: deviceId `Xami-A1-<id>`, firmware`{version,build,release}`, bloque de tiempo. (cambia salida /device)

### P2 · Nuevas ENTRADAS/SALIDAS de API (servicios de firma que faltan)
- [ ] **Bloque 8 — Forma 1**: `POST /v1/signatures/digest` (el cliente arma el PAdES y manda solo el hash -> el mini firma -> devuelve firma+cert). Envoltorio fino sobre la cola ya existente. (nueva API)
- [ ] **Compatibilidad UANATACA**: `/signbox/api/v1/sign` con payload homologado. (nueva API de entrada; homologación con cliente real)
- [ ] **Bloque 4 — Verification-as-a-Service**: endpoint para verificar firmas/documentos. (nueva API)

### P3 · Cambian salida / identidad (menos urgente)
- [c] **Bloque 3 — /device como VC W3C 2.0 (COSE)**: `/device` emite `proof` COSE_Sign1/ES256 (did:key, credentialSubject + custodiedCredentials), firma raw del device. + `GET /device/challenge` (nonce frescura). Modulo `attestation/`. Core validado byte-a-byte vs pycose. (cambia salida)
- [~] **XAMI_MODEL**: AVANCE: `XAMI_MODEL` en `version.h` (#ifndef, default "A1"), usado en deviceId. FALTA: pasarlo a Kconfig y renombrar el SoftAP. (firmware)

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
- **Code-ready (firmware, PENDIENTE de UN build+flasheo)**: B1 /health, B2 /device, B3 VC COSE + /device/challenge, y la custodia .p12 con **RSA+P-256** y **multi-cert** (tipo por credencial, job reporta `algorithm`/valida `sigType`). Ver CHANGELOG_PENDIENTES.md y PLAN_CUSTODIA_FASES.md.
- **Pendiente contrato de API (P1/P2/P3)**: B0 envelope `{meta,data}`+`timeUnix` (parcial), Forma 1 digest, UANATACA, verificación.
- **Pendiente interno (P4)**: ceremonia CA, Fase C (VaultStamping + rotación KMaster), provisioning, auditoría, zkSNARK, **sellado B7 stamping.io**.
- **Pendiente SERVER (no firmware)**: API que manda `credentialId`/`sigType` al job, R3 PAdES (RSA/ECDSA según `algorithm`), verificador de la VC, proof COSE en /health y /audit.
