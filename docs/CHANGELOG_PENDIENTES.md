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
