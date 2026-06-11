# Xami — Plan de Trabajo (Ejecución)

> El "cuándo y en qué orden". El QUÉ y POR QUÉ están en CHANGELOG_PENDIENTES.md
> y CONTEXTO_FIRMADO_BLOCKCHAIN.md — aquí solo se REFERENCIAN, no se duplican.
> Marcar [x] al completar. Fecha inicio: 2026-06-10

## Reparto de tareas
- **Claude (server/manyao):** código en repo, builds CI, config servidor, pruebas de endpoints.
- **Usuario (hardware):** flasheo del dispositivo, validación en Acrobat, decisiones de
  criterio, pruebas con clientes reales.

---

## FASE 1 — Validar el núcleo (desbloquea todo lo demás)

### 1.1 Prueba de firma real end-to-end  [CHANGELOG: "TRABAJO EN CURSO: SERVICIOS DE FIRMA"]
- [ ] Script/flujo: server genera token HMAC con el secret → POST /sign al device → firma
- [ ] Confirmar firma DER + certificado de vuelta
- [ ] Validar que el digest firmado corresponde (no doble hash)
- Responsable: Claude prepara, Usuario valida con device físico

### 1.2 Bloque 0 — Timestamp UTC + NTP  [CHANGELOG: BLOQUE 0]
- [ ] Sincronización NTP en arranque del firmware
- [ ] Función central de envelope {meta,data} con time/timeUnix/timeSynced
- Responsable: Claude (firmware + optimizer)

---

## FASE 2 — Base rápida y visible

### 2.1 Bloque 1 — /health  [CHANGELOG: BLOQUE 1]
- [ ] version/build/release, device con modelo, cert state, NTP time
### 2.2 Bloque 2 — /device  [CHANGELOG: BLOQUE 2]
- [ ] deviceId con modelo, firmware estructurado, bloque de tiempo
### 2.3 Modelo XAMI_MODEL en Kconfig  [CHANGELOG: "Familia de productos"]
- [ ] Definir XAMI_MODEL (default A1), aplicar a device + SoftAP

---

## FASE 3 — Valor de negocio (firma + clientes)

### 3.1 Bloque 8 — Servicios de firma + homologación UANATACA  [CHANGELOG: BLOQUE 8]
- [ ] Clases internas: SigningService, PadesBuilder, AuthValidator
- [ ] /v1/signatures/digest (Servicio 1)
- [ ] /v1/signatures/pdf (Servicio 2, con appearance/control)
- [ ] Compatibilidad /signbox/api/v1/sign (payload UANATACA)
- [ ] Flujo async job/result
- [ ] Prueba PAdES real en Acrobat (Nivel A) — Usuario valida
### 3.2 Bloque 7 — Sellado stamping.io  [CHANGELOG: BLOQUE 7]
- [ ] Llamada con bearer (ya probado), guardar coordenadas en respuesta (no almacenar)
- [ ] attestation en respuestas de info externa

---

## FASE 4 — Seguridad avanzada (lo ambicioso)

### 4.1 Bloque 5 — Autorización 2 capas + VaultStamping  [CHANGELOG: BLOQUE 5 + EVOLUCIÓN]
- [ ] HMAC hardening (anti-replay persistente NVS)
- [ ] VaultStamping en Python (reimplementar vaultStamping.js)
- [ ] Secreto cifrado en reposo + KUser efímero + HMAC que amarra todo
### 4.2 Provisioning del secret  [CHANGELOG: "PENDIENTE CRÍTICO"]
- [ ] Compartir secret device↔server de forma segura (no por red)

---

## FASE 5 — Identidad y auditoría (grandes)

### 5.1 Bloque 3 — /device como VC W3C 2.0 (COSE)  [CHANGELOG: BLOQUE 3]
### 5.2 Bloque 4 — Verification-as-a-Service  [CHANGELOG: BLOQUE 4]
### 5.3 Bloque 6 — Auditoría persistente y atestada  [CHANGELOG: BLOQUE 6]

---

## EN PARALELO (no bloquean, cuando haya hueco)
- [ ] Recuperación de red completa (escalera + portal con contexto) [CHANGELOG: "DISEÑO — Recuperación de red"]
- [x] Bug: /health colgaba (conexion directa al device, imposible con NAT) -> ahora via registro heartbeats
- [x] Bug: /openapi.json 500 (forward-ref LoadCertRequest) -> resuelto, /docs OK
- [ ] Borrar código viejo /home/fileserver/.../serverHSM tras validar api.xami.run
- [ ] Ceremonia CA (Nivel B verde en Acrobat)
- [ ] Subir vaultStamping.js y colección UANATACA a docs/referencias/

---

## PROGRESO GLOBAL
- Fase 1: ✅ Bloque 9 match VALIDADO en hardware real (device emparejado, secret compartido)
- Bloque 10 (polling firma): SOFTWARE LISTO (server probado+desplegado, firmware-v35 build OK).
  Pendiente: flasheo v35 + e2e firma real (1.1) -> valida la firma end-to-end.
- Bugs en paralelo /health y /openapi.json: RESUELTOS.
- Fase 2: ☐
- Fase 3: ☐
- Fase 4: ☐
- Fase 5: ☐
