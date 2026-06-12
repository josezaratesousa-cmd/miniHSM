# Pedidos al API (optimizador) desde Console

> Cosas que console NECESITA del optimizador/firmware pero que NO toca (las hace
> el otro asistente). console solo consume el api como guardián.
> Estado: lista viva. Fecha: 2026-06-11.

## P1. Cola persistente (no en RAM)
La cola de trabajos (`job_queue._JOBS`) vive en memoria (dict por device_id).
Implicaciones: se pierde si el optimizador reinicia, y obliga a --workers 1 (DT7).
PEDIDO: persistir la cola (BD/disco) para sobrevivir reinicios y permitir varios
workers. Mientras tanto, la VERDAD persistente de los pendientes vive en xami_db
(sign_requests); console no depende de la cola en RAM para el estado.

## P2. Respetar device "inhabilitado" al encolar (DT3)
Hoy POST /devices/{id}/jobs encola sin verificar si el device está bloqueado.
PEDIDO: que el encolado (y/o el heartbeat) respete el estado inhabilitado y
responda "inhabilitado", para que console pueda apoyarse en esa barrera.

## P3. Frontera del guardián (DT9)
Hoy cualquiera que alcance el optimizador puede encolar. console valida permisos
en su BD, pero el optimizador no exige que la llamada venga SOLO de console.
PEDIDO: definir cómo asegurar que el optimizador solo acepte llamadas de console
(red interna, token de servicio, etc.).

## Notas de integración (lo que console SÍ usará, sin cambios al api)
- Firmar PDF completo: POST /v1/signatures/pdf (recibe PDF + prefs, orquesta el
  polling con el device internamente y devuelve el PDF firmado). Es el endpoint
  correcto para el flujo de firma del panel (NO el de jobs crudo).
- Estado de un trabajo de PDF: GET /v1/signatures/pdf/{pid} y /pdf/{pid}/download.
- Estado de devices: GET /devices/ y /devices/{id}.

## P4. Opacidad = FONDO del sello (acordado 2026-06-12)
Hoy en pades_polling.py: image_opacity -> background_opacity (opacidad de la
IMAGEN), y text_opacity -> color gris del texto (g=1-op). El usuario decidio que
la opacidad debe aplicarse a un FONDO del sello (un recuadro detras de imagen+texto),
no a la imagen ni al texto. El otro asistente ajustara el motor para que exista un
fill_opacity / fondo del sello configurable. El simulador de console ya asume este
comportamiento: una sola "Opacidad del fondo" que pone un fondo blanco semitransparente
detras del contenido del sello. Sincronizar params cuando el API lo soporte.

## P4-FINAL: parametro fill_opacity (nombre definitivo, acordado 2026-06-12)
Nombre del parametro: fill_opacity (float 0.0-1.0, default 0.0 = transparente).
Dibuja un fondo blanco solido detras de TODO el sello (imagen+texto) con esa
opacidad. NO altera image_opacity ni text_opacity (siguen igual). Default 0.0 =>
firmas existentes no cambian. Form param: fill_opacity: float = Form(0.0).
console YA usa fill_opacity en el editor (renombrado de bg_opacity). Cuando el API
lo soporte, el preview coincidira pixel-perfect.
