# Plan de implementación por fases — Chip de custodia (.p12 multi-credencial)

> El "qué/por qué" está en `DISENO_CUSTODIA_P12.md`. Aquí va el "cuándo/orden".
> Objetivo: chip **multi-certificado** + **ceremonia de transferencia del .p12** +
> **firma masiva** con una credencial elegida del chip.
> Regla CI: tocar `firmware/**` dispara build en GitHub Actions -> **avisar antes de push de firmware**.

## Mapa de dependencias (orden obligado)
- **NTP** (hora) debe ir **antes** de TOTP.
- **Almacén multi-credencial** debe ir **antes** de firma-con-credencial y de la ceremonia.
- **Firma-con-credencial** debe ir **antes** de la firma masiva por lote.
- **Ceremonia** necesita: almacén + ECIES de transporte (ya existe el primitivo en `match_engine`).
- **Opción 2 (eFuse/flash encryption)** va **al final** (endurecimiento), tras estabilizar todo.

## Inventario de primitivas en el firmware (qué hay / qué falta)
- HKDF-SHA256: **existe** (`mbedtls/hkdf` en match_engine).
- AES-GCM: **existe** (`mbedtls/gcm`).
- ECIES (descifrar blob hacia la priv del device): **existe** (`match_ecies_decrypt`). Reutilizable.
- PKCS#12 (.p12): **falta** wiring (mbedtls lo soporta; hay que integrarlo).
- TOTP (HMAC-SHA1 + base32 + ventana): **falta**.
- Merkle root (SHA-256): **falta** (trivial sobre el SHA-256 ya disponible).
- chip_secret encapsulado (`vault_get_chip_kek_secret`): **falta** (Opción 1 = random en NVS).

## FASE 0 — Prerrequisitos (firmware)
**Objetivo:** base de tiempo y cripto lista.
- **NTP**: sincronizar hora al conectar WiFi; exponer `timeSynced` (sirve también al Bloque 0 del plan general).
- `vault_get_chip_kek_secret()`: Opción 1 (clave aleatoria propia en NVS; una sola función para migrar luego).
- Añadir helpers que faltan: TOTP, base32, Merkle root.
- **Entregable testeable:** hora correcta + tests unitarios de TOTP/Merkle con vectores conocidos.

## FASE 1 — Almacén multi-credencial (firmware: nuevo `custody_manager`)
**Objetivo:** guardar/leer N credenciales cifradas, sin tocar aún ceremonia ni firma.
- NVS indexada: `cust_<id>_priv` (cifrada), `cust_<id>_cert` (PEM), `cust_<id>_totp` (semilla),
  `cust_<id>_meta` (alias/dueño, fingerprint, vigencia, salt+nonce+tag) + **índice** de credenciales.
- API: `custody_add/list/get_cert/get_meta/delete/count`.
- Cifrado en reposo: `AES-256-GCM(KEK, priv)`, `KEK = HKDF(passphrase || chip_secret || salt)`.
- **Dimensionar la partición NVS** (`partitions.csv`) para N credenciales.
- **Entregable testeable:** alta/baja/listado de credenciales inyectadas; round-trip cifra/descifra con passphrase.

## FASE 2 — Firma con credencial seleccionada
**Objetivo:** firmar con una credencial precargada (aún sin ceremonia ni TOTP), eligiendo cuál.
- Firmware: `vault_sign_with(cred_id, digest, passphrase, ...)` -> descifra priv de esa
  credencial, firma, zeroiza.
- Firmware: `process_signing_job` lee `credentialId` del job, llama `vault_sign_with`, y
  devuelve **el cert de esa credencial** (no el del device).
- Server: el endpoint de firma acepta `credential_id` y lo incluye en el job hacia el chip.
- **Entregable testeable:** firmar un PDF con una credencial de prueba precargada (Fase 1) + passphrase.

## FASE 3 — TOTP (enrolamiento + validación)  [depende de FASE 0: NTP]
**Objetivo:** segundo factor de autorización por credencial.
- Firmware: generar semilla TOTP **por credencial**, guardarla cifrada; validar código
  (HMAC-SHA1/base32, ventana de tiempo, **anti-replay de ventana**).
- Firmware (UI local): generar `otpauth://` y mostrar **QR**.
- Integrar TOTP como **gate** de la firma (el job trae el código; el chip valida antes de firmar).
- **Entregable testeable:** enrolar Google Authenticator y exigir código válido para firmar.

## FASE 4 — Ceremonia de transferencia del .p12 (AP aislado)
**Objetivo:** cargar un .p12 de forma aislada, verificada, confidencial y autorizada.
- Firmware: modo **ceremonia** -> baja internet, levanta **AP**, sirve la UI local, espera el
  paquete con el **secreto de ceremonia** (un solo uso).
- Firmware: recibir el **.p12 cifrado (ECIES)** -> abrir PKCS#12 -> validar priv<->cert/vigencia
  -> `custody_add` (Fase 1) -> enrolar TOTP (Fase 3) -> zeroizar. Retorno a WiFi normal (timeout/botón).
- Server: orquestar inicio (emitir secreto de ceremonia, entregar **pubkey + fingerprint** al cliente).
- UI/JS: **comparar fingerprint** (bloquea si no coincide), **cifrar ECIES** el paquete, guiar el cambio de red.
- **Entregable testeable:** ceremonia completa de punta a punta cargando una credencial real.

## FASE 5 — Firma masiva por lote (Merkle) + selección en UI
**Objetivo:** firmar N documentos con un solo TOTP, atado al lote.
- Server: **Merkle root** del lote; armar paquete `{lote_id, TOTP, passphrase, nonce}` cifrado
  para el chip; transportar (cartero ciego); recibir N firmados.
- Firmware: re-calcular Merkle root y comparar con `lote_id`; validar TOTP + **nonce-challenge
  (anti-replay)**; firmar los N con `vault_sign_with`; devolver.
- UI (/firmar + consola): selector **"firmar como <credencial>"** + acordeón **"Huella del lote"**.
- **Entregable testeable:** firmar un lote de N PDFs con una sola autorización.

## FASE 6 — Atestación blockchain + transparencia
**Objetivo:** prueba de autorización + sellado de tiempo verificable por cualquiera.
- Atestar el **Merkle root** del lote (enlaza con Bloque 7 stamping.io del plan general).
- Campo `info` con la **estructura del árbol** (hojas/ramas) -> reconstrucción y **prueba de inclusión**.
- Publicar **solo el root**, nunca contenido. Definir la cadena.
- **Entregable testeable:** verificar externamente que un lote fue autorizado y sellado en el tiempo.

## FASE 7 — Endurecimiento (Opción 2)  [al final]
**Objetivo:** resistencia física real.
- Migrar `vault_get_chip_kek_secret()` a **eFuse + HMAC** del ESP32-S3.
- Activar **flash encryption** (+ secure boot). Requiere **re-enrolar** las credenciales.
- Proteger semilla TOTP en NVS, anti-replay persistente, auditoría de eventos de custodia.
- **Entregable testeable:** un volcado de flash no revela ninguna clave.

## Hitos / agrupación sugerida
- **MVP funcional (firma con custodia, sin aislamiento total):** Fases 0 -> 1 -> 2 -> 3.
- **Producto completo (ceremonia segura + masivo):** + Fases 4 -> 5.
- **Confianza pública (verificable + endurecido):** + Fases 6 -> 7.

## Reparto de trabajo
- **Firmware** (Claude escribe; usuario flashea/prueba; avisar antes de cada push -> CI build):
  Fases 0,1,2,3,4,5 (parte device), 7.
- **Server / UI** (Claude; sin disparar build): Fases 2,4,5 (orquestación), 6.
- Cada fase: implementar -> registrar en `CHANGELOG_PENDIENTES.md` -> commit/push -> probar en hardware.
