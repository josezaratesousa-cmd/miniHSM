# Análisis de impacto: custodia/agente en console

Tras la evolución del firmware (custody_manager, agent_crypto) y el server
(/v1/credentials, params credentialId/fingerprint/auth en /v1/signatures/pdf),
console pasa de un modelo "1 device = 1 clave" a "device = HSM multi-credencial".

## Estado actual de console
- Solo conoce device_id (firmar con la clave del device). NO tiene credenciales,
  fingerprints, ceremonias ni modos. Tabla devices: device_id, estado, firmware.
- sign_requests tiene device_id + design_id, NO credential_id/fingerprint.

## Impacto por capa
| Área | Impacto | ¿Rompe lo hecho? |
|------|---------|------------------|
| Firma con device_id (actual) | Ninguno | No, sigue funcionando |
| Modelo de datos (tablas) | Alto | Aditivo (nueva tabla credentials) |
| Selector "Firmar con" | Alto conceptual | Se enriquece |
| sign_send | Bajo | Ya casi listo, solo +params |
| Ceremonia de alta credencial | Alto (nuevo) | Funcionalidad nueva entera |
| Firma modo autorización (TOTP) | Medio | Nuevo paso al firmar |

## Lectura estratégica
- Nada de lo construido se rompe. device_id sigue siendo el camino base.
- La custodia NO es "un param más": es cambio de producto (gestión de identidad
  digital custodiada en hardware). Toca modelo mental, datos y agrega la ceremonia.

## Secuencia recomendada
1. AHORA: cerrar flujo device estándar (drag-drop posición) y dejarlo pulido E2E.
2. DESPUÉS (fase propia): gestión de credenciales custodiadas — tabla + UI de
   ceremonia (wizard) + selector "Firmar con" enriquecido + TOTP al firmar.
   Requiere su propio diseño y mockups.

## Lo que el server ya ofrece (para cuando se aborde)
- POST /v1/credentials/ceremony/start : inicia alta; el chip recibe la ceremonia
  por el heartbeat y entra en modo AP con el secreto.
- /v1/signatures/pdf acepta credential_id, credential_cert (PEM), fingerprint, auth.
- Modos por credencial: agente (sin TOTP) | autorizacion (TOTP por firma).
- custody_manager (chip): hasta 16 slots, RSA-2048/4096/EC, resuelve por fingerprint.
