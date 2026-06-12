# Xami — Registro de Cambios y Pendientes

> Documento de trabajo. Anota cambios probados, pendientes y decisiones de diseño.
> Producto: **Xami** (nombre interno del proyecto: miniHSM)
> Última actualización: 2026-06-10

---

## Estado actual del proyecto

El dispositivo **Xami-A1** está operativo y validado de punta a punta:

- Firmware compila en GitHub Actions (ESP-IDF v5.4.4) y publica binario en Releases
- Portal cautivo de configuración WiFi funciona (SoftAP `Xami-<DeviceID>`)
- El dispositivo se conecta a la red configurada y levanta servidor HTTP
- Heartbeat al serverHSM funciona con validación TLS (crt_bundle)
- Endpoint `/health` responde correctamente

**Device de pruebas actual:** `af811cc685297a80`
**Build validado:** #20 (release a2027a5)
**serverHSM:** https://fileserver.locker/serverHSM/api/

---

## Familia de productos Xami

Cada modelo es un **binario distinto** (funcionalidades diferentes según hardware):

| Modelo | Hardware | Generación de claves |
|--------|----------|---------------------|
| **Xami-A1** | Solo ESP32-S3 | Software (NVS cifrado) |
| **Xami-A2** | ESP32-S3 + ATECC608B | Hardware (clave no sale del chip) |
| **Xami-A3** | A2 + componentes adicionales | Hardware + extras |
| A4, A5... | futuros | espacio para crecer |

El modelo se define como opción de compilación (`XAMI_MODEL`), no se detecta en runtime.

---

## BLOQUE 1 — Mejoras a `/health` (pendiente de implementar)

**Estado actual del endpoint:**
```json
{"status":"ok","uptime":199,"opCount":0,"firmware":"1.0.0","secureBoot":false}
```

**Estructura objetivo:**
```json
{
  "status": "ok",
  "device": "Xami-A1-af811cc685297a80",
  "model": "A1",
  "firmware": {
    "version": "1.0.0",
    "build": "20",
    "release": "a2027a5"
  },
  "cert": "self-signed",
  "uptime": 199,
  "opCount": 0,
  "time": "2026-06-10T15:30:00Z",
  "timeSynced": true,
  "secureBoot": false
}
```

### Cambios concretos:

- **Fix 1 — Versión completa.** Exponer `version` + `build` + `release` (commit corto).
  Ya está cableado en CI (`XAMI_VERSION_STRING`), solo falta exponerlo en el JSON.
  Nota: `release` = hash corto del commit (NO usar la palabra "commit").

- **Fix 2 — Device ID.** Agregar `device` con formato `Xami-<MODELO>-<DeviceID>`.
  Ej: `Xami-A1-af811cc685297a80`. La función `vault_get_device_id()` ya existe.

- **Fix 3 — Estado del certificado.** Campo `cert`: `"self-signed"` o `"ca-signed"`.
  La función `cert_get_state()` ya devuelve UNPROVISIONED/PROVISIONED.

- **Fix 4 — Sellado de tiempo (NTP).** Dos partes:
  - **4a:** Agregar sincronización NTP al arranque (el ESP32 no tiene RTC con batería,
    arranca en 1970 hasta sincronizar). Servidor NTP configurable.
  - **4b:** Exponer `time` (ISO 8601) y `timeSynced` (bool) en el health.
    `timeSynced: false` = reloj aún no confiable.

- **Modelo (`XAMI_MODEL`).** Definir en Kconfig, default `"A1"`.
  El workflow puede sobrescribirlo por modelo. El nombre del SoftAP también lleva
  el modelo: `Xami-A1-<DeviceID>`.

### Conceptos aclarados:
- **secureBoot:** proteccion de hardware que solo permite ejecutar firmware firmado.
  Actualmente `false`. Activarlo es IRREVERSIBLE (quema fusibles). Se deja para una
  ceremonia controlada mas adelante (parte del modelo de seguridad L3).
- **timeSynced:** indica si el reloj ya se sincronizo por NTP y es confiable.

---

## PENDIENTES MENORES (anotados durante pruebas)

- **Portal: request abandonado.** Durante la configuración, si el usuario sale a copiar
  la contraseña y vuelve, el navegador deja un request a medias y aparece en el log:
  `httpd_parse: parsing failed` / `400 Bad Request`. No es crítico (la config se guarda
  igual), pero el portal podría hacerse más tolerante. Prioridad baja.

---

## DISEÑO DE PROVISIONING (ya implementado)

| Situación | Mecanismo | Estado |
|-----------|-----------|--------|
| Primer arranque sin credenciales | Portal Xami directo | OK |
| Red guardada conecta | Modo cliente normal | OK |
| Red no conecta (cambió router) | Fallback automático → portal (30s) | OK |
| Cambiar red a propósito (con red) | `/provision/reconfigure` remoto | Firmware OK, falta lado serverHSM |

- SoftAP abierto (sin clave) — se mejorará después con clave impresa en el equipo
- Portal autocierra tras 10 min de inactividad
- Reconfiguración remota requiere KUser admin + auditoría

---

## PENDIENTES MAYORES (para el plan de trabajo)

1. **Prueba de firma real** — validar que el Xami firma un digest (PAdES/CAdES/XAdES).
   Es el propósito central del dispositivo, aún no probado end-to-end.

2. **Lado serverHSM del `/provision/reconfigure`** — el código Python que dispara
   la reconfiguración remota. El firmware ya tiene el endpoint.

3. **Ceremonia de CA** — flujo completo: el dispositivo genera CSR, una CA lo firma,
   se carga el cert (`POST /cert`). El firmware ya tiene `cert_get_csr()` y
   `cert_load_ca_signed()`.

4. **SoftAP con clave** — mejorar seguridad del portal (clave impresa en etiqueta).

5. **Secure Boot + Flash Encryption** — activar protecciones de hardware (modelo L3).

6. **Optimizer (serverHSM)** — completar PAdES/XAdES/CAdES end-to-end con TSA.

---

## HISTORIAL DE BUILDS RELEVANTES

| Build | Cambio principal |
|-------|------------------|
| #20 | Heartbeat HTTPS con crt_bundle (validado, funcionando) |
| #19 | Desactivar PSRAM (causaba bucle de reinicio) |
| #18 | Permisos CI para crear Releases |
| #16 | Portal cautivo SoftAP + versionado automático |

---

## BLOQUE 0 (TRANSVERSAL) — Timestamp UTC estándar en TODAS las respuestas

Decisión de arquitectura: **toda** respuesta de la API (miniHSM y serverHSM) debe
incluir el timestamp UTC de forma estándar. Cada respuesta es potencialmente evidencia.

Bloque de tiempo estándar:
```json
"time": "2026-06-10T20:30:00Z",
"timeUnix": 1781112600,
"timeSynced": true
```

- **time:** ISO 8601 con `Z` explícita (UTC, inequívoco). Nunca hora local.
- **timeUnix:** epoch en segundos (para máquinas, sin ambigüedad de formato).
- **timeSynced:** si el reloj ya sincronizó por NTP. `false` = no confiar en el timestamp.

Razón del UTC siempre: dispositivos en distintos países deben hablar el mismo idioma
de tiempo. La conversión a hora local es problema de quien muestra, no de quien firma.

Implementación: función central que arma toda respuesta JSON e inyecta el bloque meta
automáticamente (evita olvidos por handler). Decidir entre:
- A) Envelope `{meta:{...}, data:{...}}` — más limpio, rompe compatibilidad actual.
- B) Campos meta al nivel raíz — menos disruptivo, mezcla meta con datos.
Recomendado: A (envelope), por estar el proyecto temprano.

Depende de: NTP (Fix 4a del Bloque 1).

---

## BLOQUE 2 — Mejoras a `/device` (pendiente)

**Estado actual:**
```json
{
  "deviceId": "af811cc685297a80",
  "firmware": "1.0.0",
  "pubkey": "04ad72e4...",
  "backend": "mbedTLS-PSA",
  "curve": "P-256",
  "sigFormat": "DER",
  "certFingerprint": "92a5dd04...",
  "certState": "UNPROVISIONED"
}
```

**Propósito del endpoint:** tarjeta de identidad criptográfica del Xami. Permite a
sistemas externos (serverHSM, CA, auditor) conocer la identidad del dispositivo y
cómo verificar lo que firma, SIN tener que firmar nada. Casos de uso:
verificar firmas (necesitan la pubkey), registro en serverHSM, ceremonia de CA,
auditoría/forense.

**Cambios:**
- `deviceId` con formato `Xami-<MODELO>-<DeviceID>` (consistente con /health)
- `firmware` estructurado (version/build/release)
- Agregar bloque de tiempo estándar (Bloque 0)
- El `backend` delata el modelo: A1="mbedTLS-PSA" (software), A2="ATECC608B" (hardware)

---

## BLOQUE 3 — `/device` como Verifiable Credential W3C 2.0 (diseño)

> miniHSM: emite la credencial y la firma (proof of possession).

Convertir `/device` en una **Verifiable Credential** alineada al
**W3C VC Data Model 2.0** (Recomendación oficial desde 15 mayo 2025).

La credencial no solo DICE la identidad, la DEMUESTRA: el Xami firma la credencial
con su clave privada. Quien verifica usa la pubkey declarada para validar la firma.
Si valida → prueba criptográfica de que el dispositivo posee la clave privada
(proof of possession). No se puede mentir sobre la pubkey sin invalidar la firma.

Estructura W3C 2.0 (3 partes: issuer/holder/verifier):
```json
{
  "@context": ["https://www.w3.org/ns/credentials/v2"],
  "type": ["VerifiableCredential", "XamiDeviceCredential"],
  "issuer": "did:xami:af811cc685297a80",
  "validFrom": "2026-06-10T20:30:00Z",
  "credentialSubject": {
    "id": "did:xami:af811cc685297a80",
    "model": "A1",
    "publicKeyMultibase": "...",
    "curve": "P-256",
    "certState": "UNPROVISIONED",
    "backend": "mbedTLS-PSA"
  },
  "proof": { ... }
}
```

**Securing mechanism elegido: COSE** (CBOR Object Signing).
Razón: binario y compacto (ideal para ESP32), estándar de facto en IoT/embebido
(pasaportes electrónicos, mDL). ECDSA P-256 = COSE ES256.
Alternativas descartadas: JOSE/JWT (texto, más pesado, útil solo para debug inicial),
Data Integrity JSON-LD (canonicalización pesada para embebido).

**Cómo COSE demuestra posesión de la clave:** la firma COSE_Sign1 solo es generable
con la privada y solo verificable con la pública declarada. Firmar = demostrar.

**Frescura (anti-replay) — soportar ambas:**
- A) Timestamp en payload: el verificador exige que sea reciente. Simple, requiere
  relojes sincronizados.
- B) Challenge-response: endpoint `/device/challenge` recibe un nonce del verificador,
  el Xami lo firma. Impredecible y de un solo uso → prueba en tiempo real. Estándar oro.

**Decisiones de diseño abiertas:**
1. DID method: `did:xami:` propio vs `did:key` (deriva de la pubkey, sin infraestructura)
2. Frescura: timestamp (A), challenge (B), o ambas (recomendado)
3. Canonicalización: cómo se serializa el payload para que device y verificador
   produzcan los mismos bytes (punto crítico donde estos esquemas suelen fallar)
4. Librería CBOR en firmware (COSE la necesita)

Specs de referencia: VC-DATA-MODEL-2.0, VC-JOSE-COSE, Data Integrity ECDSA Cryptosuites v1.0

---

## BLOQUE 4 — Verification-as-a-Service (diseño)

Servicio de verificación de firmas en el Xami Server, para que terceros que confíen
en el servidor puedan verificar sin implementar código criptográfico propio.

**Dividido en 2 piezas:**

### miniHSM (dispositivo) — POSEE la clave y FIRMA
- Emite la Verifiable Credential COSE (Bloque 3) con proof of possession
- Produce las firmas de digests (PAdES/CAdES/XAdES via optimizer)
- No verifica firmas de terceros; solo demuestra su identidad y firma
- Endpoint `/verify` actual: revisar si se mantiene o se mueve al server

### serverHSM (Xami Server) — VERIFICA y presta el SERVICIO
- Registro de dispositivos (qué pubkey = qué Xami; ya empezado con heartbeat + /device)
- Verificador COSE/ECDSA (Python: `cryptography` / `pycose`)
- Estado de certificados (self-signed / ca-signed / revocado)
- Validación de timestamp (y eventualmente contra TSA RFC3161)
- Registro de auditoría de verificaciones
- Endpoint `POST /verify` público:
```json
// request
{ "document": "...|hash", "signature": "...", "credential": "...COSE (opcional)" }
// response
{
  "valid": true,
  "signedBy": "Xami-A1-af811cc685297a80",
  "model": "A1",
  "signedAt": "2026-06-10T20:30:00Z",
  "certState": "ca-signed",
  "verifiedAt": "2026-06-10T21:00:00Z"
}
```

**Modelo de confianza (clave):** este servicio es para quien CONFÍA en el operador del
Xami Server (confianza trasladada al servidor, no solo matemática). Para partes que NO
confían (disputa legal, regulador), la verificación independiente SIGUE siendo posible
porque usamos estándares (ECDSA P-256, COSE, W3C VC 2.0) — no se encierra a nadie.

**Decisión abierta:** ¿la verificación vive en el dispositivo o en el server?
Recomendado: en el server (más recursos + registro de dispositivos).

Valor de negocio: baja la barrera de adopción, centraliza la lógica correcta,
permite agregar valor (revocación, cadena CA, auditoría), modelo cobrable por uso.

---

## BLOQUE 5 — Autorización en dos niveles (KUser/token)

Dos pruebas de autorización complementarias, no excluyentes. Se encadenan en capas.

### Comparación

| | Prueba 1 (HMAC) | Prueba 2 (VaultStamping/KUser) |
|---|---|---|
| Qué prueba | Conocimiento del secret | Posesión que produce descifrado real |
| Tipo | Declarativa (coincide) | Funcional (descifra) |
| Atribución | Débil (cualquiera con secret) | Fuerte (cada KUser único por R) |
| Verificable por tercero | No (confías en el dispositivo) | Sí (con zkSNARK, sin revelar secretos) |
| Costo en ESP32 | Bajo (ideal) | Alto (aritmética 512-bit; zkSNARK solo en server) |
| Uso | Día a día, rápido | Alto valor / no-repudio auditable |

### Prueba 1 — HMAC (ya implementada)
- `HMAC-SHA256("minihsm:<ts>:<nonce>", secret)` con secret de 32 bytes en NVS
- Protecciones: ventana 30s + HMAC válido + anti-replay
- **HARDENING: quitar `GET /token` en producción.** Hoy el dispositivo regala tokens
  válidos sin autenticar = puerta trasera total. En producción el token lo genera el
  serverHSM (que tiene el secret), nunca el dispositivo. El Xami solo VALIDA, no emite.
- Pendiente: anti-replay persistente en NVS (hoy en RAM, se pierde al reiniciar)

### Prueba 2 — VaultStamping / KUser (diseño, archivo: vaultStamping.js)

Esquema de cifrado multiplicativo modular que DESACOPLA custodia de autorización.

**Generación de claves:**
- `KMaster = K_base.p` (base secreta + primo grande) — la autoridad
- `KUser = K_user_inv.R` derivado de KMaster con R aleatorio (único por operación)
- Matemática: `K_user = (K_base · R) mod p`; se guarda su inverso modular + R

**Cifrado/descifrado:**
- Cifrar con KMaster: `C = (M · K_base) mod p`
- Descifrar con KUser: `M = (C · K_user_inv · R) mod p`
- KMaster nunca aparece en el descifrado (el desacople)

**Qué se cifra (M):** el PERMISO concreto de la operación. Ej:
`M = "sign:digest=a3f8...:device=af811cc:exp=1781112600"`
M es un vale de autorización que describe exactamente qué se autoriza.

**Quién tiene qué (decisión confirmada):**
- **KMaster en el serverHSM** (autoridad emisora). El servidor cifra los permisos.
- El Xami recibe `{digest, C, KUser}` y DESCIFRA para validar.
- Si M' descifrado es un permiso bien formado que coincide con el digest → autoriza.
- Un KUser falso produce basura al descifrar → el descifrado ES la prueba.

**Por qué es "posesión demostrable":** el token no solo coincide, DESCIFRA. Solo un
KUser legítimamente derivado del KMaster real recupera M. Cada KUser es único (por R)
→ atribuible. El zkSNARK encima permite demostrar a un tercero que la autorización
fue válida SIN revelar KUser ni KMaster → no-repudio verificable independientemente.

**Reparto por hardware (zkSNARK):**
- Validación por descifrado (aritmética modular 512-bit): SÍ en el ESP32 (pesada pero factible)
- Generación del zkSNARK (Groth16/Poseidon): SOLO en el servidor (inviable en ESP32)
- Verificación del zkSNARK: servidor o cualquier tercero (relativamente ligera)

**Nota de seguridad honesta:** el cifrado multiplicativo es lineal → débil si se
observan muchos pares (M,C) con el mismo KMaster. Apto para autorización efímera
(KUser de un solo uso, corta vida), NO para cifrar datos sensibles persistentes.

### Integración de las dos pruebas (capas)

No compiten, se complementan. Un request de firma de alto valor lleva ambas:

```json
POST /sign
{
  "digest": "a3f8...",
  // Capa 1 — HMAC (transporte: integridad + frescura + anti-replay)
  "token": "c8dde1cb...",
  "timestamp": 1781112600,
  "nonce": "3735927714",
  // Capa 2 — VaultStamping (autorización demostrable)
  "C": "...",                 // permiso cifrado por el serverHSM
  "kuser": "K_user_inv.R"     // para que el Xami descifre el permiso
}
```

**Orden de validación en el Xami:**
1. HMAC primero (barato): ¿íntegro, fresco, no replay? Si falla → rechaza ya.
2. VaultStamping después (caro): descifra C con kuser. ¿El permiso autoriza ESTE
   digest? Si sí → firma.

El HMAC es el portero rápido que filtra basura; VaultStamping es la validación
profunda de autorización real. Se gasta el cómputo caro solo en requests que ya
pasaron el filtro barato.

- HMAC solo: rápido, pero autorización = "confía en que validé"
- VaultStamping solo: demostrable, pero caro y sin protección de transporte/replay
- Ambos: cada uno hace lo que mejor hace

---

## ACTUALIZACIÓN — Decisiones tomadas (sesión 2026-06-10, parte 2)

### Autenticación vs Autorización (terminología clarificada)
- **HMAC = autenticación:** demuestra QUIÉN manda el request (el serverHSM legítimo,
  que conoce el secret). Prueba de identidad del emisor.
- **VaultStamping/KUser = autorización:** demuestra que ese request tiene PERMISO para
  firmar ESE digest (el permiso descifrado lo dice). "Alguien le dio la llave."
- El miniHSM valida ambas antes de firmar: 1) ¿te reconozco? 2) ¿tienes permiso?

### GET /token ELIMINADO (hecho — commit e7095f0)
- Era puerta trasera: regalaba HMAC válido sin autenticar.
- Decisión: el server SIEMPRE genera el token, el dispositivo SOLO valida.
- El optimizer (client.py) YA tiene la generación: HMAC-SHA256("minihsm:{ts}:{nonce}", secret).
- Cálculo idéntico firmware ↔ server, ya verificado.
- Nota: `policy_generate_token` queda en el firmware pero ya no se expone a la red.

### NUEVO PENDIENTE CRÍTICO — Provisioning del secret HMAC
El secret se genera aleatorio en el dispositivo (primer boot). El server necesita ESE
mismo secret para generar tokens válidos. Problema: hoy existe `policy_get_secret_hex`
que podría exponerse por la red = OTRA puerta trasera (exponer el secret es tan malo
como regalar tokens).
- **A resolver:** cómo se comparte el secret de forma segura dispositivo ↔ server.
- Ideal: en fábrica/provisioning, NO por la red en producción.
- Revisar si hay endpoint que expone el secret y quitarlo también.

### Modelo de operación (decisión abierta)
¿Todas las firmas exigen 2 capas (HMAC+VaultStamping), o hay operaciones de solo-HMAC
y otras de alto valor con ambas? Recomendado: Modelo B (dos niveles), VaultStamping
solo para alto valor / no-repudio auditable.

---

## BLOQUE 5 — EVOLUCIÓN: Secreto cifrado en reposo + KUser efímero (diseño afinado)

> Evolución del esquema de autorización tras análisis detallado (sesión 2026-06-10).
> Reemplaza el modelo simple de "token HMAC suelto" por uno blindado.

### Concepto central
El secreto de sesión NO se guarda en claro en el dispositivo. Se guarda CIFRADO, y
solo se puede usar si el server envía el KUser efímero correcto que lo desbloquea.
El secreto en reposo es INÚTIL sin autorización viva (el KUser de cada operación).

### Emparejamiento (match — una sola vez por device)
```
server: S = random_seguro(pequeño)               // clave de sesion, pequeña a proposito
server: secreto_cifrado = VaultStamping_encrypt(S, KMaster_device)
server → mini: secreto_cifrado
mini: guarda secreto_cifrado
```
- Cada device tiene su PROPIO KMaster (distinto por dispositivo).
- **ANOTADO: cuando llegue el ATECC608B, el secreto_cifrado va en el chip de seguridad,
  no en NVS.**

### Cada operación de firma
```
SERVER ARMA:
  - deriva KUser efimero del KMaster (UNICO, nunca repetido)
  - mensaje = "minihsm:<ts>:<nonce>:<kuser>:<ts_kuser>"   // ORDEN FIJO (canonico)
  - HMAC = HMAC-SHA256(mensaje, secret)
  - token_cifrado = AES256(payload, S)
  → manda { digest, token_cifrado, kuser, ts, ts_kuser, nonce, HMAC }

MINI VALIDA (en orden):
  1. ¿hash(kuser) ya usado? → anti-replay (buffer rotativo 10000) → si repetido, RECHAZA
  2. ¿ts y ts_kuser dentro de ventana? → caducidad → rechazo TEMPRANO (antes de gastar cripto)
  3. reconstruye el MISMO mensaje canonico
  4. recalcula HMAC y compara → si no coincide → RECHAZA
  5. S = VaultStamping_decrypt(secreto_cifrado, kuser)
  6. token_real = AES256_decrypt(token_cifrado, S)
  7. valida token_real
  8. si todo OK → registra hash(kuser) en buffer → FIRMA
```

### Propiedades de seguridad logradas
- **Secreto inútil en reposo:** sin KUser efímero, secreto_cifrado no se puede usar.
  Robar el flash/NVS no sirve de nada.
- **Replay imposible (reciente):** cada KUser único, su hash se quema en buffer rotativo.
- **Replay imposible (antiguo):** aunque el buffer rote y olvide un hash viejo, el
  timestamp caducado lo rechaza igual. Las dos barreras se cubren mutuamente.
- **No se puede alterar ni mezclar piezas:** el HMAC cubre token + KUser + AMBOS
  timestamps. Cambiar el KUser, su timestamp, o mezclar piezas de operaciones
  distintas → el HMAC no coincide → rechazado.
- **Autorización demostrable:** "el server me autorizó y puedo probarlo" (VaultStamping).
- **Autenticación fuerte:** el HMAC es la verificación criptográfica robusta.
- **Barato en ESP32:** secreto pequeño + AES256 (hardware) + HMAC (hardware).

### El hueco que se encontró y se cerró (importante)
ANÁLISIS: el KUser, si viaja FUERA del HMAC, es modificable. Un atacante podría
cambiarlo o mezclar el KUser de una operación con el token de otra, y el HMAC (que
solo protegía su propio token) no lo notaría.
SOLUCIÓN: incluir el KUser y su timestamp DENTRO del mensaje que firma el HMAC.
Regla general: **todo lo que importa va dentro del HMAC. Lo que queda fuera, es
modificable.** Un solo HMAC con un solo sello protege token + KUser + timestamps.

### Detalles de implementación a cuidar
- **Canonicalización estricta:** server y mini deben armar el mensaje byte por byte
  idéntico (mismo orden, separadores, formato). Si difieren, el HMAC no coincide aunque
  todo sea legítimo. Punto crítico donde estos esquemas suelen fallar.
- **Buffer anti-replay:** 10000 hashes × 32 bytes = ~320KB en NVS. Reservar partición.
- **Caducidad temprana:** validar timestamps en paso 2 (antes de descifrar) ahorra
  cómputo en requests inválidos/ataques.
- **Tamaño del secreto:** pequeño a propósito. VaultStamping aquí da DEMOSTRABILIDAD,
  no fortaleza; la fortaleza criptográfica la aporta el HMAC. Secreto pequeño mitiga
  la debilidad del cifrado lineal multiplicativo.
- **KMaster por device:** cada dispositivo con su propio KMaster en el server.

---

## LIMPIEZA DE API — endpoints del miniHSM (actualizado)

Endpoints eliminados por seguridad/redundancia:
- **GET /token** — ELIMINADO (puerta trasera; el server genera tokens, el mini valida)
- **GET /pubkey** — ELIMINADO (redundante; la pubkey vive en /device, su lugar natural)

Endpoints actuales del miniHSM:
```
POST /sign                    firma un digest (requiere autorizacion)
POST /verify                  verifica una firma
GET  /cert                    certificado actual (PEM)
POST /cert                    carga cert firmado por CA
GET  /csr                     genera CSR para ceremonia de CA
GET  /device                  identidad criptografica (incluye pubkey)
GET  /health                  estado operativo
GET  /audit                   log de auditoria
POST /provision/wifi          configura WiFi (desde portal)
DEL  /provision/wifi          borra credenciales WiFi
POST /provision/reconfigure   reconfiguracion remota (requiere KUser admin)
```

Principio aplicado: una sola fuente de identidad (la fuerte). La pubkey en /device,
que evolucionara a Verifiable Credential con proof of possession (Bloque 3).

---

## BLOQUE 7 — Sellado de tiempo en blockchain (stamping.io)

Atestación de tiempo independiente y pública vía blockchain (TSA real). Resuelve la
debilidad del reloj del dispositivo: el sello en blockchain no se puede falsificar
ni retroceder. Es el "sellado TSA" que estaba en la documentación original (PAdES LT/LTA).

### Configuración (probado y funcionando)
- **STAMPING_API_KEY** en el `.env` del serverHSM, global para todos POR AHORA.
- Después: endpoint `/auth` de tenant para crear bearer tokens por tenant.
- **El token va como BEARER en el header** `Authorization: Bearer <key>`, NO en el
  payload. Más seguro. PROBADO: stamping.io lo acepta así (code 200).

### Quién llama
El **serverHSM** hace la llamada a stamping.io (tiene internet, maneja la key).
El miniHSM no sale a internet para esto.

### Cuándo se sella (IMPORTANTE — definición de alcance)
El sellado ocurre **cada vez que se pide información externa al device**, no solo logs:
- Cuando se pide el **log de auditoría** → se sella la atestación del log
- Cuando se pide **información del device** (/device) → se sella esa atestación
- Regla general: **toda entrega de información externa lleva su sellado de tiempo**.
  Cada respuesta verificable queda anclada en blockchain con su momento.

### trxid = sha1(evidence) — promesa inmediata (PROBADO)
Con `async=true`, el trxid NO hay que esperarlo: es `sha1(evidence)`, calculable al
instante. Es una promesa determinística; stamping.io ejecuta el sellado en blockchain
después, bajo ese mismo trxid.
- Auto-verificable: cualquiera con el evidence recalcula sha1 y confirma el trxid.
- VERIFICADO en prueba real: trxid local == trxid devuelto por stamping.io.

### Payload (corregido — sin reference)
```
POST https://api.stamping.io/stamp/
Header: Authorization: Bearer <STAMPING_API_KEY>
Body:
{
  "evidence": "<hash firmado de la atestacion (64 hex sha256)>",
  "data": "<base64(attestation)>",
  "transactionType": "<tipo de evento>",
  "async": "true",
  "external_key": "<deviceId, ej Xami-A1-af811cc...>",
  "subject": "<descripcion legible>"
}
```
- **reference: NO se usa** (descartado el encadenamiento de trxids).
- **external_key = deviceId**: permite buscar en stamping.io todos los sellos de un
  dispositivo. Registro externo e inmutable de la actividad sellada del device.
- **evidence**: el hash firmado de la atestación que se entrega.
- **data**: base64 de la atestación completa (recuperable desde IPFS de stamping.io).

### transactionType por tipo de evento
- `xami-audit-log` → atestación de log de auditoría
- `xami-device-info` → atestación de informacion del device
- (otros segun se necesiten)

### Verificación y resultado
- Solo importa **`code == 200`** (sellado transmitido OK).
- Se guarda el **trxid** en el log/atestación como prueba de sellado de tiempo.
- El resto de campos de la respuesta (blockhash, nonce, version...) NO interesan.

### Sin fallback (decisión)
Si stamping.io está caído, no se hace el sellado. NO hay fallback. Es UNA prueba más
de atestación, no la única. La firma y el log siguen siendo válidos sin el sello.
El sistema no depende de un servicio externo.

### Secreto a provisionar (se suma a la lista)
STAMPING_API_KEY es global por ahora. Con `/auth` de tenant, cada tenant tendrá su
bearer. Se suma a: secret HMAC, secreto VaultStamping, token stamping.

### Mecanismo real de async=true (Merkle batch anchoring) — aclarado
stamping.io con async=true funciona así:
1. ENCOLA el evidence y devuelve el trxid (promesa) al instante → CERO latencia.
2. No hace nada más en ese momento.
3. Cada ~5 minutos junta todos los evidence encolados.
4. Construye un MERKLE TREE con todos ellos.
5. Registra solo el HASH RAÍZ del árbol en varias blockchains (una sola tx por lote).

Implicaciones:
- **Cero latencia:** el Xami/serverHSM solo encola y sigue, sin esperar blockchain.
- **Escala:** miles de sellos se anclan con una sola transacción (la raíz Merkle).
- **Prueba individual conservada:** con la Merkle proof (ruta del evidence hasta la
  raíz), cada sello se demuestra incluido en el lote, sin revelar los demás.
- **Garantía temporal = ventana del lote (~5 min):** el sello prueba "existía antes
  de que se cerrara este lote", no un timestamp al segundo. Aceptable para TSA.
- **Verificación profunda (opcional, para disputas):** consultar a stamping.io la
  Merkle proof del evidence tras cerrar el lote → evidence → ruta → raíz → tx blockchain.
  Para uso normal, guardar el trxid basta.

### Datos a guardar para la prueba de estampado (CORRIGE "solo trxid")
El mecanismo completo de stamping.io es:
1. evidence encolados → Merkle tree
2. Merkle tree (con datos del JSON) → se sube a IPFS
3. El CID de IPFS → se registra en un contrato
4. Un contrato de INDEXACIÓN relaciona (recipient + blockhash + nonce + timestamp)
   → permite calcular/ubicar el CID
5. Con el CID → recuperas el Merkle tree de IPFS → verificas tu evidence

Por tanto el trxid SOLO NO basta para la verificación profunda. Hay que guardar las
**coordenadas de la prueba** (bloque "stamping" junto a cada sello):
```json
"stamping": {
  "trxid": "<sha1 del evidence>",
  "recipient": 534070,
  "blockhash": "0x...",
  "nonce": "...",
  "timestamp": 1781115838778
}
```
Estos 5 campos juntos permiten al algoritmo de atestación (del grupo, que entrará)
calcular el CID, ir a IPFS, traer el Merkle tree y probar que el evidence estaba
incluido. El resto de campos de la respuesta (version, server, quantity...) NO interesan.

Naturaleza: el sello es una PRUEBA DIFERIDA Y RECONSTRUIBLE. Meses después, con esas
coordenadas → CID → Merkle tree en IPFS → prueba matemática de inclusión.

### CORRECCIÓN IMPORTANTE — El Xami NO almacena el sello, solo lo RESPONDE
Rol del Xami respecto al sello (decisión de arquitectura):
- El Xami **atesta** (manda el evidence a stamping.io a sellar)
- **Confía** en que stamping.io hará su trabajo (anclar en blockchain)
- **Entrega las coordenadas del sello en la respuesta** (trxid, recipient, blockhash,
  nonce, timestamp) al que preguntó, en ese mismo momento
- **NO almacena NADA — ni el trxid.** Es stateless respecto al sello.

Por qué es lo correcto: el Xami NO debe ser la fuente de verdad de su propia prueba.
- No puede falsificar la prueba (no la custodia)
- No puede borrar la prueba (no la tiene)
- La prueba existe independientemente del Xami (en blockchain + IPFS via stamping.io)
- El verificador valida contra stamping.io/blockchain, SIN depender del Xami

La CUSTODIA de la prueba es del VERIFICADOR, no del Xami. El Xami transmite y olvida.
La memoria de la prueba vive en blockchain.

Implicaciones (simplifica):
- No persistir sellos (ni en miniHSM ni en serverHSM) → menos storage, menos código
- Flujo stateless: pides info → Xami atesta → responde con sello → fin
- Cada respuesta lleva su atestación FRESCA (pedir el log mañana = sello nuevo)
- El verificador guarda el bloque "stamping" que le importe, si quiere validar luego

Estructura de respuesta (data + attestation firmada + stamping, todo al vuelo):
```json
{
  "data": { ...log o device... },
  "attestation": { "device","issuedAt","hash","signature" },
  "stamping": { "trxid","recipient","blockhash","nonce","timestamp" }
}
```
Esto REEMPLAZA la nota anterior de "guardar las coordenadas". El Xami NO guarda.

---

## BLOQUE 8 — Servicios de firma del serverHSM (homologados con UANATACA)

> Objetivo de negocio: que los clientes que hoy usan UANATACA solo cambien el HOST
> y sigan funcionando. Payload compatible + respuesta similar. Campos extra de Xami
> (deviceId, attestation, zkproof) son OPCIONALES: si no los leen, no les afecta.

### Patrón de arquitectura (decisión)
Separar lógica interna de interfaz pública:
```
ENDPOINTS (interfaz, patrón REST correcto)
   ├── Compatibilidad UANATACA: /signbox/api/v1/sign, /job/{id}, /result/{id}
   └── API nativa Xami:         /v1/signatures/digest, /v1/signatures/pdf
        │ usan (NO duplican lógica)
   CLASES INTERNAS REUTILIZABLES
   ├── SigningService   → orquesta la firma
   ├── PadesBuilder     → arma el PAdES (pyhanko, ya esbozado en pades.py)
   ├── MiniHSMClient    → habla con el dispositivo (ya existe)
   └── AuthValidator    → valida HMAC + KUser (Bloque 5)
```
Las dos "caras" comparten la misma lógica. Solo cambia qué exponen.

### Familia 1 — Compatibilidad UANATACA (clientes existentes)
Path EXACTO de UANATACA para que solo cambien el host:
```
POST /signbox/api/v1/sign      → firma (acepta payload UANATACA tal cual)
GET  /signbox/api/v1/job/{id}  → estado del job (async)
GET  /signbox/api/v1/result/{id} → PDF firmado
```

Payload UANATACA soportado (mismos nombres de campo):
- `file` / `url_in` / `url_out` → PDF (archivo o por URL)
- `format` (default PADES), `level` (default BES = B-B)
- `signature_subfilter` (ETSI.CAdES.detached)
- `signature_appearance` → MISMO formato JSON de UANATACA (ver abajo)
- `useasync` (true → job_id pattern)
- `tsa_url` / `tsa_user` / `tsa_pass` → sellado de tiempo
- `username` / `password` / `pin` → IGNORADOS (la clave está en el miniHSM, no en nube)
- `otp` / `sessionid` → para flujo con validación de usuario (futuro)

Campos EXTRA de Xami (opcionales, default = comportamiento UANATACA esperado):
- `deviceId` → qué Xami firma (si no se manda, device por defecto del tenant)
- (en respuesta) `attestation`, `zkproof` → el plus de Xami, UANATACA no los tiene

### signature_appearance (formato UANATACA adoptado tal cual)
```json
{
  "text": ["Firmado por: %(CN)s", "%(EMAIL)s %(L)s %(SUBJECT)s", "Fecha: %(DATE)s"],
  "date": "%d/%m/%Y %H:%M:%S %z",
  "timezone": "America/Guatemala",
  "position": "30,100,165,150",   // x,y,ancho,alto
  "qrcode": "",
  "horizontal": true,
  "page": 0
}
```
Plantillas soportadas: %(CN)s, %(EMAIL)s, %(L)s, %(SUBJECT)s, %(DATE)s, etc.
Adoptar el mismo formato = máxima compatibilidad con apps que ya lo arman.

### Familia 2 — API nativa Xami (clientes nuevos, patrón limpio)

**Servicio 1 — /v1/signatures/digest (paso-through):**
Una app ya armó el PAdES/CMS y solo necesita que el miniHSM firme el hash.
```json
POST /v1/signatures/digest
Authorization: Bearer <token>
{ "deviceId": "Xami-A1-af811cc...", "digest": "a687e3... (64 hex)", "requestId": "..." }

→ 201:
{
  "meta": { "time","timeUnix","timeSynced","device","model" },
  "data": {
    "requestId": "...",
    "signature": "3045...",
    "algorithm": "ECDSA-P256-SHA256-DER",
    "certificate": "-----BEGIN CERTIFICATE-----...",
    "certState": "UNPROVISIONED"
  },
  "attestation": { ... },   // firma del Xami sobre la operacion + sello stamping.io
  "zkproof": { ... }        // prueba "me autorizaste" (KUser, Bloque 5)
}
```

**Servicio 2 — /v1/signatures/pdf (asistencia completa PAdES):**
La app manda el PDF y datos; el server hace todo el trabajo PAdES.
```json
POST /v1/signatures/pdf
Authorization: Bearer <token>
{
  "deviceId": "Xami-A1-af811cc...",
  "pdf": "<base64>",
  "requestId": "...",
  "visible": true,                    // flag visible/invisible
  "appearance": {                     // si visible=false, se ignora
    "image": "<base64>", "page": 0,
    "position": "30,100,165,150",
    "text": ["...","..."]
  },
  "metadata": { "reason","location","contact","name" }
}

→ 201:
{
  "meta": { ... },
  "data": {
    "requestId": "...",
    "signedPdf": "<base64>",
    "control": {                      // CONTROL: qué se firmó y qué se aplicó
      "signedHash": "f4b943...",
      "algorithm": "ECDSA-P256-SHA256-DER",
      "certState": "UNPROVISIONED",
      "visible": true,
      "appliedMetadata": { "reason","location","contact","name" },
      "appearance": { "page","position" }
    }
  },
  "attestation": { ... },
  "zkproof": { ... }
}
```

### attestation + zkproof en TODA respuesta de firma (punto clave)
Tanto el server como el mini siempre responden "me autorizaste y lo demuestro":
- **attestation**: firma del Xami sobre la operación (proof of possession, Bloque 3)
  + las coordenadas del sello stamping.io (trxid, recipient, blockhash, nonce,
  timestamp — Bloque 7). El Xami NO las almacena, solo las responde.
- **zkproof**: la prueba de conocimiento cero del KUser (VaultStamping, Bloque 5)
  que demuestra que la operación fue autorizada, sin revelar secretos.
Compatible con UANATACA: si la app no lee estos campos, no le afecta.

### Por qué la homologación funciona (concepto)
UANATACA = firma remota con certificado en la nube.
Xami = firma con HSM físico + atestación blockchain + zkproof.
PERO de cara a la app, el contrato se ve IGUAL: manda PDF + apariencia, recibe PDF
firmado. A la app no le importa el "cómo" interno. Cambian host y funciona, con el
plus de Xami de regalo.

### Estado del código actual (diagnóstico)
- `optimizer/signing/pades.py` (161 líneas): base correcta con pyhanko. El patrón
  MiniHSMPdfSigner(Signer) delega la firma al miniHSM. Flujo criptográfico CORRECTO
  (verificado: el mini usa psa_sign_hash que firma el digest directo, sin doble hash).
- `optimizer/api/main.py`: tiene /sign/digest (casi listo para Servicio 1) y /sign/pdf
  (recibe archivo, devuelve PDF — falta migrar al patrón nuevo).
- FALTA: los dos servicios con el patrón nuevo, la compatibilidad UANATACA, los bloques
  attestation+zkproof, el flag visible/invisible, la apariencia, el bloque control.
- PENDIENTE: prueba end-to-end con Acrobat (Nivel A = válida pero identidad amarilla,
  alcanzable con cert autofirmado actual; Nivel B verde = requiere ceremonia CA).

### Decisiones confirmadas
1. /digest y /pdf separados (Opción A).
2. Path exacto UANATACA (/signbox/api/v1/sign) para cambio solo de host.
3. signature_appearance formato UANATACA adoptado tal cual.
4. Flujo async job/result replicado (la firma puede tardar, habla con device físico).
5. attestation + zkproof en toda respuesta de firma.

---

## BUG ENCONTRADO Y CORREGIDO — Reconexión WiFi (commit a20786a)

**Síntoma:** tras una caída temporal del WiFi, el mini quedaba muerto con bucle de
`esp-tls: Failed to create socket` / `heartbeat: ESP_ERR_HTTP_CONNECT`, sin
recuperarse aunque el WiFi volviera. Requería reinicio manual.

**Causa:** el handler de WIFI_EVENT_STA_DISCONNECTED reintentaba solo WIFI_MAX_RETRY
(5) veces y luego marcaba WIFI_FAIL_BIT, rindiéndose PARA SIEMPRE. Correcto en el
arranque (password mala → no insistir), fatal en operación (corte temporal del router
más largo que 5 intentos → muerto permanente).

**Fix (Opción A — reconexión indefinida):** distinguir arranque vs operación con el
flag `s_connected_once`:
- Arranque inicial (nunca conectó): reintenta 5 veces → si falla, portal de config.
- Caída en operación (ya había IP): reconecta INDEFINIDAMENTE. El router vuelve y el
  mini se recupera solo, sin reiniciar.
- No bloquea el event loop (reconexión inmediata vía el ciclo de eventos, sin delays).
- Log cada 10 intentos para no saturar consola.

---

## DISEÑO — Recuperación de red completa (escalera + portal con contexto)

> Amplía el fix básico de reconexión infinita (commit a20786a). PENDIENTE de implementar.

### Escalera de recuperación
```
ARRANQUE (primera vez, SIN credenciales guardadas):
  → portal VACÍO → usuario configura red + contraseña desde cero

OPERACIÓN, pierde WiFi (YA tenía credenciales, la red EXISTE):
  → desconecta limpio → reconecta
  → reconexión INFINITA (nunca se rinde; la red existe, el router volverá)
  → si persiste fallando (~5 min continuos): se REINICIA el dispositivo
  → al arrancar, intenta conectar con las credenciales guardadas (~5 min)
     → si conecta: opera normal
     → si NO conecta: abre portal CON CONTEXTO
```

### Los DOS modos de portal (diferencia clave)
La diferencia entre el portal del primer arranque y el que aparece después es
**qué datos ya tiene el dispositivo** (puede o no cambiar las credenciales):

**Portal primer arranque (sin credenciales):**
- No hay red guardada. Portal vacío, configurar desde cero.
- No hay nada que "mantener".

**Portal tras perder red (con credenciales):**
- Ya tiene una red guardada (ej. ATENEA).
- Portal CON CONTEXTO: muestra "no encuentro tu red ATENEA".
- **CONSERVA las credenciales (no las borra automáticamente).**
- Ofrece al usuario: **reintentar / mantener / cambiar de red**.
- Si elige mantener/reintentar → usa las que ya tiene (evita reescribir la clave
  por un simple corte de luz).
- Si elige cambiar → reemplaza las credenciales.

### Principio
El dispositivo se esfuerza al máximo por reconectarse solo (reconectar → reiniciar),
y solo cuando de verdad no puede, pasa el control al usuario — pero sin hacerle
reescribir todo: le muestra qué pasó y le da la decisión.

### Estado actual
- HECHO (commit a20786a): reconexión infinita en operación (no se rinde tras 5 intentos).
- PENDIENTE: la escalera con tiempos (5 min → reinicio → 5 min → portal), el disparador
  de reinicio por tiempo, y el portal con contexto (mensaje "no encuentro la red" +
  conservar credenciales + opciones reintentar/mantener/cambiar).

---

## MIGRACIÓN DE SERVIDOR — fileserver.locker → api.xami.run (HECHO 2026-06-10)

Migración con corte (el viejo ya no se usa). Nueva infraestructura:

| Pieza | Antes | Ahora |
|-------|-------|-------|
| Dominio API | fileserver.locker/serverHSM/api | **https://api.xami.run** |
| Path en disco | /home/fileserver/public_html/serverHSM | /home/xami/public_html/api |
| Usuario | fileserver | xami |
| systemd service | minihsm-optimizer (puerto 8181) | **xami-optimizer** (puerto 8182) |
| Proxy Apache | serverHSM.conf | userdata/.../xami/api.xami.run/optimizer.conf |
| Cuenta cPanel | fileserver | xami (grupo xami) |

Pasos ejecutados:
1. Copiado repo + .env (chmod 600) a /home/xami/public_html/api, ownership xami:xami
2. venv RECREADO (no copiado, por rutas absolutas) con Python 3.11.9 + deps exactas
3. Service xami-optimizer.service creado (user xami, puerto 8182, EnvironmentFile .env)
4. Proxy Apache api.xami.run/ → 127.0.0.1:8182 (excluye /.well-known para SSL)
5. Firmware actualizado: CONFIG_SERVERHSM_URL = https://api.xami.run (3 archivos)
6. Portal WiFi muestra el dominio api.xami.run en el header
7. Viejo service detenido y deshabilitado (no borrado, queda como respaldo)

Ruta del heartbeat conservada: firmware arma <url>/devices/heartbeat → el router
del optimizer tiene prefix /devices. Verificado: responde 422 (espera datos) = ruta OK.

NOTA: la llave de deploy de git sigue en /home/manyao/.ssh/minihsm_github. El push
desde el repo de xami se hace con esa llave (vía sudo) + safe.directory para root.

### Pendiente de la migración
- Borrar el código viejo en /home/fileserver/public_html/serverHSM cuando se confirme
  que api.xami.run funciona 100% con el dispositivo real (reflasheo + heartbeat).
- Reflashear el dispositivo con el firmware nuevo (apunta a api.xami.run) y validar
  que el heartbeat llega al nuevo server.

### Bugs preexistentes detectados durante la migración (no de la migración)
- `/health` del optimizer se cuelga si el device no está accesible (timeout largo en
  get_client().health()). Debería tener timeout corto y responder igual.
- `/openapi.json` da error 500: pydantic `LoadCertRequest is not fully defined`
  (falta un .model_rebuild() o el forward ref del modelo). Afecta /docs schema.

---

## BLOQUE 6 — Auditoría persistente, robusta y atestada (diseño)

> Diseñado en conversación, formalizado aquí. PENDIENTE de implementar.

### Estado actual del /audit
- Buffer circular en RAM de solo 64 entradas (AUDIT_LOG_SIZE=64).
- Cada entrada firmada individualmente por el device.
- Campos: ts (relativo, no UTC), op, reqId, result, deviceId, sig.

### Carencias detectadas
1. **Persistencia:** está en RAM → se pierde al reiniciar. Un atacante podría reiniciar
   para borrar el rastro.
2. **Capacidad:** solo 64 entradas → la 65a borra la 1a. Fácil de saturar.
3. **Timestamp relativo:** ts viene de esp_timer (desde el arranque), no es hora real.
   Se arregla con NTP (Bloque 0/1).

### Mejoras de diseño (aportadas en conversación)
1. **Firmar el LOG COMPLETO al consultarlo** (no solo cada entrada). Cierra el fraude
   por OMISIÓN: si alguien quita entradas, la firma del conjunto se rompe. El Xami
   certifica "este es el log completo que te entrego, en este momento".
2. **Cursor por fecha:** si el cliente manda una fecha → devuelve desde ahí. Si no
   manda nada → devuelve el último tramo. Paginación para el log rotativo.
3. **Canonicalización:** firmar el hash del log con formato estándar (quitar espacios,
   orden fijo) tipo JCS/RFC 8785, para que el verificador reproduzca el mismo hash.

### Atestación del log (aporte adicional)
La firma del conjunto debe incluir, para ser inequívoca y no reusable:
- hash del conjunto de entradas devueltas
- rango cubierto (from/to)
- momento de la atestación (timestamp UTC)
- opcional: nonce/challenge del que consulta (anti-reuso)

Estructura propuesta:
```json
{
  "log": { "from","to","count","entries":[...con firma individual...] },
  "attestation": { "device","issuedAt","logHash","signature" }
}
```
Dos niveles de firma: cada entrada (integridad individual) + el conjunto (integridad
y completitud). Se integra con el sellado stamping.io (Bloque 7): la atestación del
log se sella en blockchain.

### Integración con Bloque 5 (anti-replay)
El buffer anti-replay del KUser (10000 hashes rotativos) tiene el MISMO problema de
persistencia. Misma solución: persistir en NVS. Reservar partición (~320KB).

---

## BLOQUE 9 — Emparejamiento (match) y provisioning de secretos

Mecanismo por el que un Xami se da de alta en el server y recibe sus secretos.
Resuelve: como comparten el HMAC secret device y server.

### Modelo de confianza (por modelo)
- **Xami-A1 (sin chip):** confianza con verificacion a posteriori.
  - El server CONFIA en la pubkey que envia el device (no la tenia de antes).
  - Pero verifica que el deviceID sea uno de los VENDIDOS (lista de legitimos).
  - Si alguien engana: se detecta y se BLOQUEA ese deviceID.
  - Primera config la hace un empleado de confianza (simplificacion para arrancar).
- **Xami-A2/A3 (con chip ATECC):** MATRICULADOS de fabrica. La pubkey nace en el chip
  y se registra al fabricar -> garantia criptografica fuerte.
- El modelo de seguridad ESCALA con el hardware.

### Flujo del match (A1)
1. Mini arranca con internet -> POST /devices/match {deviceId, pubkey, ip, firma}
2. Server verifica: deviceID en lista de vendidos? firma valida contra pubkey?
   (proof of possession: solo el dueno de la privada pudo firmar)
3. Si cuadra: genera secretos (HMAC; luego KMaster, token stamping), los CIFRA
   con la pubkey del mini, los envia en la respuesta
4. El mini los descifra con su PRIVADA (nunca salio) y los guarda en NVS
5. Ambos comparten el HMAC secret -> server genera tokens, mini valida

### Propiedades
- Secret viaja CIFRADO con la pubkey del mini -> solo el mini lo descifra.
- La clave privada NUNCA viaja (raiz de confianza).
- Server verifica identidad antes de dar secretos. Puede BLOQUEAR un deviceID.

### Detalle tecnico (importante)
La pubkey es EC P-256, NO RSA. Las claves EC no cifran directo. Para "cifrar con la
pubkey" se usa ECIES: ECDH efimero + clave simetrica derivada + AES-GCM. Estandar.

### Piezas (Fase 1)
- Firmware: endpoint de match, descifra secretos con su privada, guarda en NVS.
- Server: /devices/match verifica deviceID + proof of possession, genera y cifra
  secretos con ECIES, responde.
- Lista de vendidos (simple por ahora; dev = device propio).

### Pendiente produccion
- Lista de vendidos real (BD) + flujo de bloqueo. A2/A3 matricula de fabrica en chip.

### Bloque 9 — Estado de implementación (Capa 1 y 2 hechas)
- Capa 1 (server): /devices/match verifica deviceID vendido + proof of possession
  (ECDSA P-256 sobre challenge "deviceId:ts:nonce"). PROBADO e2e: legitimo OK,
  no-vendido 403, firma falsa 401. Modulos: sold_devices.py, crypto_match.py.
- Capa 2 (ECIES): cifrado de secretos. Esquema EXACTO (server y firmware deben coincidir):
  * ECDH efimero P-256
  * HKDF-SHA256, salt = 32 bytes de ceros (explicito), info = "xami-match-v1"
  * AES-256-GCM, IV 12 bytes, tag 16 bytes
  * blob = eph_pub(65) || iv(12) || ciphertext || tag(16), todo hex
  * Server (crypto_match.py: ecies_encrypt) PROBADO round-trip.
  * Firmware (match_engine.c: match_ecies_decrypt) con mbedTLS. Validar al compilar/flashear.
- PENDIENTE: cablear el endpoint de match en el FIRMWARE (que el device llame al server
  en el arranque, reciba el blob, lo descifre con match_ecies_decrypt, guarde el secret).
- PENDIENTE: que el server /devices/match GENERE el secret y lo devuelva cifrado (hoy
  Capa 1 solo verifica identidad; falta sumar ecies_encrypt del secret en la respuesta).

### Nota tecnica: funciones mbedTLS en ESP-IDF
mbedtls_hkdf y similares NO se compilan por defecto. Al usar una funcion mbedTLS
nueva hay que habilitar su CONFIG_MBEDTLS_*_C en sdkconfig.defaults o da
"undefined reference" en el LINK (no en compilacion). El match necesito
CONFIG_MBEDTLS_HKDF_C=y. ECDH/ECP/GCM/MD ya venian por defecto.

### Bloque 9 — VALIDADO EN HARDWARE REAL (2026-06-11)
Match completo funcionando en device fisico (build 33, commit c0fdf74).
Log: ecies_decrypt ret=0, secret provisionado y guardado en NVS, MATCH OK.
Server: device matriculado automaticamente + pubkey TOFU + secret entregado.
Cadena de fixes que tomo llegar aqui:
1. CONFIG_MBEDTLS_HKDF_C=y (link de mbedtls_hkdf)
2. match en task dedicada stack 8192 (evita stack overflow en main)
3. matricula automatica en vez de lista de vendidos previa
4. lectura HTTP con open/write/read (perform consumia el stream)
5. RNG en mbedtls_ecdh_compute_shared (NULL daba ECP_BAD_INPUT_DATA -20352)
Ambos lados comparten el mismo HMAC secret. Fase 1 (nucleo) lista.

## BLOQUE 10 — Conectividad server->device (polling de trabajos)

Problema: el server (api.xami.run) NO puede iniciar conexion HACIA el device:
esta detras del NAT del router del cliente. El server solo ve la IP publica del
router (ej 190.238.103.77), que no reenvia al device privado (192.168.x).
Probado: curl a la IP publica da timeout (NAT bloquea entrada).

DECISION: modelo POLLING (como el correo). El device pregunta por trabajos; el
server nunca inicia conexion. Ventajas: cero infra extra, cero problema de NAT,
escala sin esfuerzo, robusto ante reinicios. El heartbeat YA es un polling.

Flujo:
1. Device hace poll: GET /devices/{id}/jobs (cada ~20-30s o junto al heartbeat)
2. Server responde: vacio, o { requestId, digest, kuser(token), ts, nonce }
3. Device valida token (HMAC del match) y firma el digest -> POST resultado
4. Usuario sube doc en xami.run (logueado) -> server encola trabajo para ese device
5. Aceleracion (idea usuario): si esta apurado, JS de la web pide refresh inmediato
   (como el boton Actualizar del correo).

Descartado por ahora: WebSocket directo (N conexiones, no escala) y MQTT con
broker (infra extra). Polling = lo mas simple que resuelve el problema real.
SERA LO PRIMERO A IMPLEMENTAR para probar la firma end-to-end.

### Bloque 10 — Diseño de implementacion (REUSA EL HEARTBEAT, no endpoint nuevo)

DECISION (aclarada 2026-06-11): el poll NO es un GET /jobs separado. Se REUSA el
heartbeat existente (POST /devices/heartbeat), que YA es el polling. La respuesta
del heartbeat pasa a llevar 2 cosas nuevas: el job (si hay) y nextPollSeconds.

POR QUE reusar el heartbeat (no es solo comodidad): el token del job lo genera el
SERVER y lo valida el device con policy_validate_token, que exige timestamp dentro
de ventana +/-30s contra SU reloj. Pero el reloj del device es esp_timer (segundos
desde arranque), NO UTC (NTP/Bloque 0 pendiente). El server no conoce ese uptime...
salvo que el device se lo acaba de mandar EN el heartbeat (campo timestamp). Asi el
server acuna el token usando ESE timestamp -> el device valida 1 fraccion de seg
despues -> diff ~0, dentro de ventana. Un GET /jobs separado no tendria ese reloj.

nextPollSeconds (ritmo dictado por el server):
- El server decide el intervalo y lo manda en CADA respuesta de heartbeat.
- El device lo guarda en RAM (la global s_interval_sec de heartbeat.c) y lo usa
  para programar el proximo poll. Al reboot no esta en RAM -> usa el default
  compilado (DEFAULT_INTERVAL_SEC=300) hasta el primer heartbeat, que lo redicta.
- Abre la puerta a configurarlo por panel web (por device o global) sin reflashear.

SERVER - piezas:
- minihsm/job_queue.py: cola en RAM thread-safe por device. Job = {requestId,
  deviceId, digest, status, result(sig+cert), createdAt}. Estados:
  PENDING -> DELIVERED -> DONE/ERROR. Sobrevive nada al reinicio (RAM), ok para
  e2e; persistir en JSON queda para despues si hace falta.
- Heartbeat (POST /devices/heartbeat) extendido: tras registrar IP, busca job
  PENDING del device. Si hay: acuna token FRESCO con device_secrets.get_secret()
  usando como timestamp el body.timestamp del heartbeat (clave: reloj del device),
  nonce nuevo aleatorio, kuser=HMAC-SHA256(secret,"minihsm:{ts}:{nonce}"). Marca
  DELIVERED. Responde job={requestId,digest,kuser,ts,nonce}. Y nextPollSeconds.
- POST /devices/{id}/jobs           encolar (lo llama la web al subir doc). -> requestId
- POST /devices/{id}/jobs/{rid}/result  device postea sig+cert -> DONE.
- GET  /devices/{id}/jobs/{rid}     la web consulta estado/resultado.

FIRMWARE - heartbeat.c:
- Subir MAX_RESP_LEN (256 -> 768) para que entre el job.
- Parsear resp JSON: leer nextPollSeconds -> s_interval_sec. Leer job -> si hay,
  validar kuser con policy + firmar digest con vault_sign + POST result.
- OJO stack: vault_sign (ECDSA) puede no caber en stack 4096 del heartbeat (el
  match necesito task dedicada 8192). Evaluar: subir stack del heartbeat o
  despachar el firmado a task dedicada. Decidir al implementar firmware.

E2E: web encola -> device recoge en su heartbeat -> firma -> postea result ->
web lee result. Primera validacion de firma real end-to-end (Fase 1.1).

### Bloque 10 — AVANCE: SERVER hecho y probado (2026-06-11)
- minihsm/job_queue.py: cola RAM thread-safe. enqueue/next_pending/mark_delivered/
  set_result/get. Estados PENDING->DELIVERED->DONE/ERROR. Probado unitario OK.
- api/devices.py: heartbeat extendido (pickup de job + nextPollSeconds=25) +
  _mint_kuser (HMAC "minihsm:{ts}:{nonce}" con secret del match, ts=reloj del device
  del propio heartbeat) + 3 endpoints: POST /devices/{id}/jobs (encolar),
  POST .../jobs/{rid}/result, GET .../jobs/{rid}.
- Probado e2e logico (sin HTTP, venv no tiene httpx): kuser coincide byte a byte
  con la formula del firmware, ts=reloj device, segundo poll no re-entrega,
  ciclo completo hasta DONE. Rutas registradas OK.
- PENDIENTE server: el service xami-optimizer corre el codigo VIEJO en RAM; hay que
  reiniciarlo para que tome los cambios (antes de probar con device real).
- PENDIENTE: lado FIRMWARE (heartbeat.c) - parsear job+nextPollSeconds, firmar, postear.

### Bloque 10 — AVANCE: FIRMWARE hecho (2026-06-11) - pendiente build+flash
heartbeat.c reescrito:
- Stack 4096->8192 (firma inline; 8192 = numero probado en match).
- MAX_RESP_LEN 256->768 (cabe el job).
- Lectura HTTP cambiada a open/write/fetch_headers/read (perform consumia el
  stream, igual que el fix del match). Antes la resp se ignoraba.
- Parsea nextPollSeconds -> s_interval_sec (RAM). Al reboot vuelve a 300.
- Parsea job -> process_signing_job: valida kuser (policy_validate_token),
  crypto_hex_to_bytes(digest), vault_sign -> DER, cert_get_pem, y postea a
  POST /devices/{id}/jobs/{rid}/result {signature,cert,status}.
- Cierra el cliente del heartbeat ANTES de firmar/postear (1 TLS a la vez).
- Buffers grandes (sig_hex, cert_pem, url, resp_buf) en static (no stack).
DECISION: firma inline en la task del heartbeat (no task dedicada) por simplicidad;
si en hardware da stack overflow, migrar a task dedicada 8192 como el match.
build v33->v34: primer intento fallo por %u vs uint32_t (fix PRIu32, commit e50827d). Rebuild en curso.
PENDIENTE: reiniciar xami-optimizer.service para que el server tome el codigo nuevo.

### Bloque 10 — BUILD OK: firmware-v35 (2026-06-11)
Build CI success tras fix PRIu32. Release firmware-v35 con minihsm-merged.bin.
Server ya corriendo codigo nuevo (xami-optimizer reiniciado, rutas verificadas).
PENDIENTE (usuario): flashear v35 (merged.bin @ 0x0) + e2e firma real:
  1. encolar: POST /devices/<id>/jobs {digest} -> devuelve requestId
  2. device recoge en su heartbeat (5s tras boot, luego cada 25s), firma, postea
  3. consultar: GET /devices/<id>/jobs/<requestId> -> status DONE + signature DER + cert
Device de prueba: fe4dfede3b10c54b (ya emparejado, secret en NVS/server).

### Bugs en paralelo — RESUELTOS (2026-06-11, solo server)
- /openapi.json daba 500: LoadCertRequest se usaba como forward-ref string en
  load_ca_cert ANTES de definir la clase (pydantic v2 no la resolvia). Fix: mover
  DigestSignRequest+LoadCertRequest arriba (antes de su uso) y desquotar la
  anotacion. /openapi.json y /docs ahora 200.
- /health se colgaba (timeout ~8s+): llamaba get_client().health(), que intenta
  CONECTAR directo al device — imposible con NAT (Bloque 10). Fix: /health ahora
  reporta optimizer + devices desde el registro de heartbeats (online/segundos),
  sin conexion directa. Responde en ~80ms. Aplicado con restart del service.

## BLOQUE 5+10 UNIFICADO — Autorizacion con AUTORIDAD EN EL MINI (diseño 2026-06-11)

> Cierra el modelo de autorizacion + transporte tras analisis largo. REEMPLAZA el
> modelo previo donde el KMaster vivia en el server. PENDIENTE de implementar.

### Inversion clave
En polling el MINI inicia (pide su cola), asi que el mini es la AUTORIDAD: tiene la
clave privada (firma) y ademas su propio KMaster (autoriza). El mini autoriza al
server a entregarle la cola; el SERVER debe poder DEMOSTRAR que el mini lo autorizo
(no al reves). Antes lo teniamos al reves (KMaster en el server) = incorrecto.

### Claves y donde viven
- Mini: clave privada EC (identidad/firma; NVS; PERSISTENTE) + su propio KMaster
  (256 bits; ROTADO en cada arranque; de sesion).
- Server (por device): C = VaultStamping_encrypt(S, KMaster_mini) — el "codigo
  cifrado" que el mini le entrega. El server NUNCA guarda S ni el KMaster.

### VaultStamping (multiplicativo, de vaultStamping.js)
- KMaster = (K_base, p), p primo. KUser = (K_user_inv, R), R aleatorio por operacion,
  K_user_inv = (K_base*R)^-1 mod p. UNICO por operacion.
- encrypt: C = (S*K_base) mod p.   decrypt: S = (C*K_user_inv*R) mod p.
- Da DEMOSTRABILIDAD, no fortaleza (la fortaleza la pone el HMAC). 256 bits, S<p.
- OJO seguridad: quien tiene C y S puede derivar K_base = C*S^-1 mod p. Por eso el
  server NUNCA persiste S (lo recupera en memoria por op y lo olvida).

### Match (una vez) + ROTACION por arranque
- Mini genera KMaster, elige S, calcula C. Match: server->mini entrega secretos
  (HMAC etc., ECIES como ya esta); mini->server entrega C.
- En CADA arranque el mini ROTA KMaster (primo 256 nuevo -> nuevo S -> nuevo C1) y en
  el aviso OBLIGATORIO de "me resetee" (que el panel de xami.run muestra al usuario)
  manda {C1, firma_EC(C1)}. El server verifica la firma EC (proof of possession),
  acepta C1 (nueva EPOCA) y DESCARTA el set de kusers de la epoca anterior. El mini
  usa el KMaster nuevo SOLO tras la confirmacion del server (orden atomico).
- Firmar el aviso con la EC: autentica C1 + impide spoof de reset (anti-DoS rotacion).

### Peticion de cola (cada poll) — pseudocodigo canonico
MINI arma:
  kuser, ts_kuser = derive_kuser(KMaster)          # R aleatorio
  ts, nonce = now(), random()
  msg  = "minihsm:"+ts+":"+nonce+":"+kuser+":"+ts_kuser    # ORDEN FIJO canonico
  hmac = HMAC_SHA256(key=S, data=msg)
  envia { kuser, ts, ts_kuser, nonce, hmac }

SERVER valida (en orden):
  if hash(kuser) in used_kusers[epoch]: reject     # 1. anti-replay (set por epoca)
  if not within_window(ts, ts_kuser): reject       # 2. frescura
  S' = vaultstamping_decrypt(C, kuser)             # 3. recupera S en memoria
  if HMAC_SHA256(S', msg) != hmac: reject          # 4. candado (valida kuser+integridad)
  used_kusers[epoch].add(hash(kuser))              # 5. registra
  entrega cola cifrada con ECIES(pubkey_mini)      # 6. solo el mini la abre
  forget(S')                                       # 7. olvida S

### Propiedades
- Doble candado: el kuser destraba S, y S verifica el HMAC. kuser falso -> S malo ->
  HMAC falla. Un solo HMAC protege kuser+timestamps+integridad (todo dentro del HMAC).
- Anti-replay en el SERVER (tiene disco), por EPOCA; se AUTO-PODA en cada rotacion
  (los kusers viejos mueren contra el C nuevo -> descifran a basura -> HMAC falla).
  El mini queda SIN estado de kusers (solo los genera). Adios particion NVS 320KB.
- Confidencialidad de la cola por ECIES -> solo el mini la abre (tapa al impostor que
  pollea con el deviceId; TLS no autentica al que pollea).
- TLS encima: canal + defensa DNS (el mini valida cert con esp_crt_bundle).
- Forward secrecy en la capa de autorizacion (KMaster efimero por sesion).

### Modelo de amenaza asumido
Server honesto-pero-curioso; la soberania del mini es contra atacantes EXTERNOS.
El server, al recuperar S en memoria + tener C, PODRIA derivar K_base de la EPOCA
actual — pero NO puede forjar FIRMAS (clave EC en el chip), y la rotacion acota
cualquier derivacion a una sesion. Blindaje total (server cripto-incapaz de forjar
autorizaciones) = el mini firmaria el poll con la EC, sin S compartido. Capa futura.

### zkSNARK (capa POSTERIOR, server-side)
Prueba Groth16 (snarkjs, vaultStamping.js) de que un kuser valido descifra C a S
(commitment Poseidon), verificable independientemente / on-chain, SIN revelar
KUser/KMaster. Es el no-repudio portable ("plus de Xami"). NO bloquea la firma; se
agrega despues. Reimplementar/portar: el multiplicativo a Python (trivial); el prover
zk como sidecar Node o equivalente Python.

### Reparto de implementacion
- SERVER (Python): VaultStamping multiplicativo, validacion del poll (5 pasos),
  anti-replay por epoca, rotacion EC-firmada (aviso de reset), entrega ECIES de la
  cola. (zkSNARK: sidecar/después.)
- FIRMWARE (C): gen KMaster (mbedtls_mpi_gen_prime 256), derive_kuser (MPI),
  aviso de reset EC-firmado con C1, HMAC con S, ECIES-decrypt de la cola (ya existe
  del match), vault_sign (ya existe). Adios anti-replay en RAM/NVS del device.

### FIRMA REAL E2E — VALIDADA EN HARDWARE (2026-06-11)
Primer ciclo completo de firma por el canal de polling, en device fisico
00da0f3b57ec8f14 (firmware-v35):
- server encola job (POST /devices/{id}/jobs) -> device lo recoge en su heartbeat
  -> valida token HMAC -> vault_sign -> postea {signature DER, cert} -> server DONE.
- Verificacion: la firma DER verifica contra la pubkey real del device (la del match,
  en sold_devices.json) usando ECDSA PREHASHED -> VALIDA. Con SHA256 normal (doble
  hash) -> invalida. CONFIRMADO: el device firma el digest directo, sin doble hash.
- nextPollSeconds 300->25 adoptado por el device en vivo (ritmo dictado por server OK).
- Fix de despliegue necesario: el service corria con --workers 2 y la cola/registro en
  RAM no se comparten entre workers (job caia en un worker, GET/poll en otro -> "not
  found"). Pasado a --workers 1 (override systemd). Para multi-worker futuro: mover
  cola+registro a store compartido/persistente.
- Cert va UNPROVISIONED (placeholder sin clave real) -> pendiente ceremonia CA para PAdES.

### BLOQUE 8 - FORMA 2 IMPLEMENTADO: /v1/signatures/pdf (2026-06-11)
Subes un PDF -> el miniHSM lo firma (PAdES via cola de polling) -> descargas el
PDF firmado. Validado e2e en hardware (device 00da0f3b57ec8f14).
Piezas nuevas (server, NO toca firmware):
- signing/dev_pki.py: CA de desarrollo PERSISTENTE (/home/xami/public_html/api/pki/,
  fuera del repo). Emite el cert del device al vuelo con su pubkey real del match.
  NO es CA de produccion; para verde-sin-importar hace falta la ceremonia CA.
- signing/pades_polling.py: PollingSigner(pyhanko.Signer). async_sign_raw encola el
  digest en job_queue y espera con asyncio.sleep (NO bloquea el event loop -> el
  heartbeat del device entra en paralelo y deposita la firma). sign_pdf_bytes() firma
  PAdES-B-B in-process (sin HTTP a si mismo, sin cliente directo).
- minihsm/pdf_jobs.py: estado RAM de trabajos de firma de PDF (guarda el PDF firmado
  mientras el cliente descarga). Distinto de job_queue.
- api/signatures.py: router /v1/signatures. POST /pdf (encola, background task,
  devuelve requestId; auto-resuelve device online), GET /pdf/{id} (estado),
  GET /pdf/{id}/download (PDF firmado), GET /ca.pem (CA de prueba).
- api/main.py: CORSMiddleware (allow_origins xami.run) + include signatures_router.
- Web: https://xami.run/firmar/ (public_html/firmar/index.html) - subir/ver/descargar.
Verificacion del PDF firmado: INTACT:TRUSTED,UNTOUCHED (firma del device verifica,
encadena a la CA dev). Patron asincrono para evitar timeouts del proxy mientras el
device pollea (~0-25s). Pendiente para PAdES verde real: ceremonia CA publica.

### FIX: PDFs con xref hibrido (2026-06-11)
Sintoma: al firmar un PDF "del mundo real" (ej. un whitepaper exportado), pyhanko
lanzaba SigningError "Attempting to sign document with hybrid cross-reference
sections while hybrid xrefs are disabled". Causa: el PDF usa secciones de
cross-reference hibridas (tabla xref clasica + /XRefStm a un stream xref, por
compat con lectores viejos) y el IncrementalPdfFileWriter en modo estricto las
rechaza (pdf_signer.py: if prev.strict and prev.xrefs.hybrid_xrefs_present -> raise).
Fix: construir el writer con strict=False en signing/pades_polling.py. pyhanko
maneja los xref hibridos de forma tolerante y, de paso, acepta mejor PDFs reales.
La firma resultante sigue siendo PAdES valida.

### BLOQUE 8 F2: firma VISIBLE + modo APPROVAL/CERTIFY (2026-06-11)
/v1/signatures/pdf ahora acepta:
- visible (bool), page (1-indexed), box ("x1,y1,x2,y2" puntos PDF), stamp_image
  (UploadFile, va de fondo del sello via PdfImage), stamp_text (placeholders de
  pyhanko: %(signer)s, %(ts)s; OJO no existe %(nl)s, usar salto de linea real),
  image_opacity (0..1).
- mode: "approval" (default, DocMDP FILL_FORMS -> se pueden agregar mas firmas) o
  "certify" (DocMDP NO_CHANGES, P=1 -> sella el documento, no mas firmas/cambios).
Motor: signing/pades_polling.sign_pdf_bytes usa signers.PdfSigner con TextStampStyle
(+ PdfImage de fondo) y new_field_spec=SigFieldSpec(on_page,box). Firma INVISIBLE si
visible=False (igual que antes).
Validado en hardware (device 00da0f3b57ec8f14):
- visible: intact/valid/trusted True, Rect del campo = box pedido [40,40,360,170].
- certify: /Perms/DocMDP presente, nivel P=1 (NO_CHANGES) confirmado.
Aclaracion: ENTIRE_FILE (cobertura del ByteRange) NO impide firmas posteriores; lo
que controla eso es approval vs certify (DocMDP). Por eso se expuso "mode", no un
toggle de ENTIRE_FILE.
Web xami.run/firmar/ actualizada con controles: modo, firma visible (pagina,
coordenadas, imagen de sello, texto). NOTA: la pagina vive en public_html/firmar/
(fuera del repo).

### BLOQUE 8 F2: separar ATRIBUTOS del diccionario vs TEXTO VISIBLE (2026-06-11)
Feedback del usuario: en firma profesional, el texto visible del sello NO es lo
mismo que los atributos del diccionario (name/reason/location/contact). Antes
'name' estaba fijo al deviceId y faltaba 'contact'; el texto visible quedaba atado
al name via %(signer)s.
Fix: /v1/signatures/pdf ahora expone los 4 atributos del SignatureObject de forma
independiente del texto visible:
  - name    -> /Name        (firmante declarado; si vacio = MiniHSM-{deviceId})
  - reason  -> /Reason
  - location-> /Location
  - contact -> /ContactInfo
El stamp_text (apariencia) es TEXTO LIBRE, independiente. Quedan 3 capas separadas:
(1) atributos del diccionario (declarativos, Propiedades en Acrobat),
(2) texto visible del sello (libre),
(3) firmante criptografico (cert del device, lo unico que da validez).
Validado en hardware: /Name=Juan Perez Quispe, /Reason, /Location, /ContactInfo
correctos en el dict, texto visible distinto, cert subject = MiniHSM-00da0f3b57ec8f14.
Web /firmar reorganizada en dos secciones: "Atributos de la firma" y "Apariencia
visible".

### BLOQUE 8 F2: stamp_source (sello = atributos | texto libre) (2026-06-11)
Feedback: el sello visible (lineas) y los atributos del diccionario deben poder ser
IGUALES o DISTINTOS, a eleccion del usuario. Nuevo parametro en /v1/signatures/pdf:
  - stamp_source="custom" (default): el sello usa stamp_text (texto libre,
    independiente de los atributos).
  - stamp_source="attributes": el server arma el sello con los 4 atributos del
    diccionario (name/reason/location/contact) + fecha, una linea cada uno:
      Firmado por: {name|%(signer)s}
      Razon: {reason}
      Lugar: {location}
      Contacto: {contact}
      Fecha: %(ts)s
Validado en hardware: en modo attributes la apariencia (AP/N del campo de firma)
contiene los textos de los atributos (Maria Lopez Tello, Conformidad, Cusco,
entidad, Contacto, Fecha). 5 lineas confirmadas.
Web /firmar: selector "Contenido del sello" (Usar atributos | Texto personalizado),
el ejemplo del textarea ahora es de 5 lineas.

### BLOQUE 8 F2: 4 fixes UX/firma (2026-06-11)
Fix1 - Sello con 3 modos (param stamp_source): attributes (usa los 4 atributos),
       default (estandar del motor: firmante+fecha), custom (texto libre).
Fix2 - TSA / sellado de tiempo RFC 3161 (param tsa_url). Usa pyhanko HTTPTimeStamper
       pasado a PdfSigner(timestamper=). Validado contra freetsa.org -> el PDF queda
       con unsigned attr 'signature_time_stamp_token' (PAdES-T). Vacio = sin TSA.
Fix3 - Web /firmar: +2px en toda la tipografia (base 15->17px) por legibilidad.
Fix4 - Disposicion de imagen del sello (param image_mode): 'background' (fondo, con
       image_opacity) o 'left' (imagen a la izquierda + texto a la derecha; se calcula
       el reparto del box ~40/60 con SimpleBoxLayoutRule + Margins). Param image_opacity
       expuesto en la web como slider.
Validado en hardware (00da0f3b57ec8f14): image_mode=left OK, TSA OK (PAdES-T).
Copia versionada de la web en optimizer/web/firmar.html (la activa vive en
public_html/firmar/index.html, fuera del repo).

### BLOQUE 8 F2: TSA select + opacidad texto + borde (2026-06-11)
- Web /firmar: TSA pasa de input libre a SELECT con TSAs estandar (FreeTSA, DigiCert,
  Sectigo, Certum) + "Personalizada" (muestra input para URL a mano) + "Sin sellado".
- Opacidad de IMAGEN y de TEXTO separadas (sliders). text_opacity<1 se implementa como
  atenuacion via TextBoxStyle.text_color=(g,g,g), g=1-opacidad (pyhanko NO soporta alpha
  real en texto; sobre fondo claro luce como opacidad, sobre imagen la aclara).
- BORDE de toda la firma (imagen+texto) con ANCHO configurable, via BaseStampStyle
  border_width (0 = sin borde). Params nuevos en /v1/signatures/pdf: text_opacity,
  border (bool), border_width (int).
Validado en hardware: firma con border_width=3 + text_opacity=0.55 + image_mode=left OK.

### BLOQUE 8 F2: image_width (proporcion imagen/texto en modo left) (2026-06-11)
Nuevo param image_width en /v1/signatures/pdf: ancho de la imagen en el modo
image_mode=left. Acepta "NN%" (fraccion del box) o "NN"/"NNpx" (puntos PDF).
Default = 40% (lo previo). Se acota a [10%,90%] del ancho del box. El texto ocupa
el resto a la derecha. Validado en hardware (image_width=30% OK).

### Web /firmar: rediseño 3 columnas + tema claro/oscuro + cURL (2026-06-11)
Layout a 100vh: header + 3 columnas (Documento+atributos basicos | Configuracion
en acordeones | Salida). Cada columna scrollea internamente; expandir un acordeon
NO estira la pagina (overflow:hidden en .cols, overflow-y:auto en cada .colbody).
Modo claro/oscuro con toggle (persistido en localStorage). Acordeon "cURL equivalente"
que se arma en vivo segun el formulario, con resaltado y botones Copiar / Descargar .sh.
Responsive: <920px apila columnas. Copia versionada en optimizer/web/firmar.html;
backup de la version vertical en public_html/firmar/index_backup_vertical.html.

### BLOQUE 8 F2: 3 fixes de apariencia del sello visible (2026-06-11)
1) PADDING: el sello tenia padding interno (margenes por defecto + box_layout_rule
   de la caja de texto de pyhanko, 10pt). Ahora margenes 0 en todos los layouts y
   box_layout_rule con margen 0 -> contenido pegado, sin marco con aire.
2) IMAGEN DEFORMADA: la causa era pasar margenes FLOAT (pyhanko usa Fraction y
   rompia el calculo). Ahora todos los margenes/dim son ENTEROS + SHRINK_TO_FIT
   -> la imagen toma el ancho pedido y la altura sale proporcional (aspecto OK).
3) CARACTERES RAROS: NO eran acentos. Con un caracter fuera de Latin-1 (guion largo
   "—", comillas tipograficas) pyhanko pasaba TODO el texto a UTF-16 con BOM y la
   fuente simple dibujaba el BOM ("þÿ") + bytes nulos (letras espaciadas). Fix:
   fuente DejaVuSans INCRUSTADA (GlyphAccumulatorFactory) -> texto por indices de
   glifo, Unicode completo, con ActualText. Requiere uharfbuzz (pyHanko[opentype]).
Validado en hardware (70% izq + acentos + em-dash): imagen 280x112 aspecto 2.5,
texto por glifos sin BOM, sin padding. Muestra: descargas/sello_70_nuevo.png
PENDIENTE: control de fondo de opacidad detras del texto (caja semiopaca).

### BLOQUE 8 F2: PAdES real (subfilter) + validacion de digest (2026-06-11)
AUDITORIA de reglas que imponiamos de mas. Corregidos:
- j) SubFilter: el meta NO seteaba subfilter -> pyhanko generaba adbe.pkcs7.detached
  (CMS basico de Adobe), NO PAdES, pese a venderse como PAdES. Se agrega
  subfilter=SigSeedSubFilter.PADES -> ahora /SubFilter = ETSI.CAdES.detached (PAdES
  real, base para PAdES-T/LTV). Verificado leyendo el /V del PDF firmado.
- h) async_sign_raw ignoraba el digest_algorithm que pide pyhanko y siempre hacia
  SHA-256 -> riesgo de desajuste silencioso (signedAttrs dirian otro algo y la firma
  saldria invalida). Ahora valida: si != sha256 lanza NotImplementedError claro
  ("el miniHSM P-256 solo soporta SHA-256"). SHA-256 sigue siendo lo correcto para P-256.
Sin cambios de firmware. Verificado en hardware (firma done, SubFilter PAdES).
PENDIENTE auditoria: a) campo Signature1 fijo (bloquea multifirma), b) location
default "Peru", d) max(image_opacity,0.9) pisa al cliente en modo left, e) approval
fuerza FILL_FORMS, f) solo 2 de 3 niveles DocMDP, g) saneo Latin-1, i) ceremonia cert.
(c) se deja: defaults con branding Xami se mantienen por decision de negocio.

### BLOQUE 8 F2: auditoria - quitar reglas de negocio impuestas (2026-06-11)
Tras la auditoria, corregidas las imposiciones nuestras (el cliente del API decide;
nosotros solo exponemos opciones reales de PAdES):
- a) CAMPO UNICO: el field_name estaba fijo en "Signature1" -> error "already filled"
  al firmar un PDF ya firmado. Ahora _unique_field_name() detecta los campos con
  enumerate_sig_fields y toma el siguiente Signature{n} -> HABILITA FIRMAS MULTIPLES.
  Verificado: PDF ya firmado acepta 2da firma (Signature1 + Signature2).
- b) location: default dejaba de ser "Peru" -> None (no asumir pais del firmante).
- d) opacidad: se quito max(image_opacity, 0.9) del modo left; se respeta la opacidad
  que pida el cliente.
- e) approval SIN DocMDP: approval ya no fuerza FILL_FORMS; docmdp_permissions=None
  para co-firmas (lo correcto). certify mantiene restriccion. Verificado DocMDP=False.
- f) NIVEL DocMDP: nuevo param certify_level (1 bloquea / 2 formularios / 3 anotaciones)
  para mode=certify. Verificado /P=3 con certify_level=3.
NO tocados por decision: c) defaults con branding Xami se mantienen. g) saneo Latin-1
queda (su arreglo era fuente incrustada, que dio el bug de espaciado). i) ceremonia
del cert: se hara al final (enrolamiento server-side, opcion B).

### BLOQUE 8 F2: fill_opacity + fill_color (fondo del sello) (2026-06-11)
Nuevos params INDEPENDIENTES en /v1/signatures/pdf:
- fill_opacity (float 0..1, default 0.0): opacidad de un recuadro de fondo solido
  detras de TODO el sello (imagen + texto), cubriendo toda la caja (dentro del borde).
- fill_color (str "#RRGGBB", default "#FFFFFF"): color del recuadro; solo aplica si
  fill_opacity>0.
Default 0.0 = transparente = comportamiento actual EXACTO (firmas existentes no
cambian: _build_stamp_style devuelve el TextStampStyle normal salvo que fill_opacity>0).
NO toca image_opacity (imagen) ni text_opacity (color del texto): son ejes distintos.
Implementacion: subclase _FillTextStamp (override render) que inyecta 'q /FillGS gs
R G B rg 0 0 W H re f Q' como CAPA BASE (tras el q inicial, antes de imagen/texto), con
opacidad real via ExtGState /FillGS (/ca,/CA). _FillTextStampStyle(frozen) override
create_stamp. Funciona en los 3 casos (sin imagen / background / left). Verificado en
hardware: stream con relleno como 1a capa cubriendo 0 0 W H. Muestra: descargas/sello_fill.png

### Web /firmar: control de fondo del sello (fill_opacity + fill_color) (2026-06-11)
Agregados en "Apariencia del sello": slider "Opacidad fondo" (id=fop, 0-100% -> fill_opacity
0..1) + color picker "Color fondo" (id=fcol -> fill_color #RRGGBB). Solo se envian si la
opacidad > 0 (default 0 = sin fondo, sin cambios). Reflejado en collect()/submit y en el
cURL dinamico. Validado: node --check OK, HTTP 200.

### CUSTODIA Fase 0 — prerequisitos firmware (2026-06-12)
Base para el chip de custodia (ver DISENO_CUSTODIA_P12.md / PLAN_CUSTODIA_FASES.md):
- Nuevo modulo `cc_helpers` (base32 RFC4648, TOTP RFC6238 HMAC-SHA1 + verify con ventana,
  Merkle root SHA-256). Logica validada contra vectores conocidos (TOTP RFC6238 x6, base32,
  Merkle) compilando el .c real con arnes OpenSSL: TODO OK, -Wall limpio.
- `vault_get_chip_kek_secret()` (Opcion 1: aleatorio 32B en NVS, encapsulado para migrar a
  eFuse en produccion). NVS key `kek_secret` en namespace `vault`.
- NTP: `network_sntp_start()` + `network_time_synced()` (prerequisito de TOTP), llamado al
  conectar WiFi en main.c. ESP-IDF v5.4.4 (esp_sntp_*).
- CMakeLists: registrado cc_helpers + REQUIRES esp_hw_support.
