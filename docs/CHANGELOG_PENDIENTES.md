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
