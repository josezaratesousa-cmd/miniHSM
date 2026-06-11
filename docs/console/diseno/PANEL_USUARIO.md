# Diseño — Panel de USUARIO (inbox de firmas)

> Especificación de diseño del panel de usuario, basada en el analisis aprobado
> (analisis/PANEL_USUARIO.md) y los mockups aprobados.
> Fase 1.C del PLAN_DE_TRABAJO_CONSOLE. Estado: DISEÑO APROBADO -> a construir.

---

## 1. Concepto

El panel de usuario es un INBOX DE CORREO para firmas. El usuario entra y ve sus
trabajos pendientes como quien ve correos sin leer. Firmar es como leer/responder;
"enviar a firmar" es como redactar un correo nuevo. Toda accion nueva abre un
PANEL LATERAL (drawer) que se desliza desde la derecha, sin sacar al usuario del
inbox.

Metafora consistente: bandejas (Pendientes/Firmados/Rechazados), redactar (Nuevo),
abrir un item (detalle con tabs), acciones (Firmar/Rechazar/Descargar).

---

## 2. Layout general

Tres zonas fijas:

- HEADER (superior, fijo): boton hamburguesa (colapsa sidebar), logo Xami,
  y a la derecha: notificaciones (campana) + avatar/menu de perfil.
- SIDEBAR (lateral izquierdo): menu por secciones con icono + texto y contadores.
  - Redimensionable arrastrando el borde derecho.
  - Ancho persistido en localStorage (clave: xami_sidebar_width).
  - Colapsable/ocultable (toggle desde el hamburguesa); estado tambien en
    localStorage (clave: xami_sidebar_collapsed).
- MAIN (area central): contenido de la seccion activa (por defecto Pendientes).
- DRAWER (lateral derecho, on-demand): panel que se superpone para "Nuevo" y para
  el detalle de un documento. El main queda atenuado detras.

---

## 3. Menu del sidebar

```
INBOX
   Pendientes (n)     -> bandeja de trabajos por firmar/rechazar
   Firmados (n)       -> historial de firmados
   Rechazados (n)     -> historial de rechazados
CONFIGURACION
   Mis dispositivos   -> lista de devices del usuario + estado
   Mis preferencias   -> preferencias de firma reutilizables
   Mi consumo         -> saldo y consumo (billing del usuario)
```

Cada item INBOX muestra un contador. El item activo se resalta. Las secciones
(INBOX, CONFIGURACION) son encabezados de grupo (acordeon ligero).

---

## 4. Bandejas / listado (tipo email)

Cada bandeja es una lista donde cada fila es un documento (como un correo). Fila
clicable: abre el detalle en el drawer.

Columnas:
| Col | Contenido | Notas |
|---|---|---|
| icono | tipo/estado del documento | file-text / file-check / file-x |
| Documento | nombre del PDF + origen/identidad | origen = device/identidad que firma |
| Tamaño | p. ej. 248 KB | alineado derecha |
| Páginas | nº de páginas | alineado derecha |
| Fecha | cambia por bandeja | ver abajo |
| Acción | según bandeja | ver abajo |

La FECHA y la ACCIÓN dependen de la bandeja:
- Pendientes -> fecha "Solicitado"; acción: Firmar / Rechazar (o badge "En cola"
  si el device está dormido y aún no lo procesa).
- Firmados   -> fecha "Firmado" (verde); acción: Descargar PDF firmado.
- Rechazados -> fecha "Rechazado" (rojo); acción: ver motivo (info).

Barra superior de la bandeja: título + contador, y a la derecha Buscar / Refrescar
/ Filtrar. Paginación al pie (la lista puede ser larga).

---

## 5. Drawer "Nueva firma" (redactar)

Se abre con el botón "Nuevo". Campos:
- Dispositivo / identidad (select de los devices del usuario).
- Documento: zona de arrastrar/soltar PDF (o clic para elegir).
- Motivo (opcional; si vacío, usa la preferencia guardada).
- Nota: "Usará tus preferencias guardadas" (link a Mis preferencias).
- Acciones: Enviar a firmar / Cancelar.

Al enviar: console valida (device del usuario, activo, saldo>0), encola en el
optimizador y crea el registro en sign_requests. El item aparece en Pendientes.

---

## 6. Drawer de detalle del documento (tabs)

Se abre al hacer clic en una fila. Dos tabs:

### Tab "Documento" (abierto por defecto)
- Vista previa del PDF (navegable por páginas).
- Acciones según estado: Pendiente -> Firmar / Rechazar; Firmado -> Descargar;
  Rechazado -> ver motivo.

### Tab "Trazabilidad (n)" (cerrado por defecto)
- Línea de tiempo vertical del ciclo de vida del documento, del evento más antiguo
  (arriba) al más reciente (abajo). Cada hito: icono + título + fecha/hora + autor.
- Eventos estándar:
  1. Enviado a firmar (cuándo, por quién).
  2. Abierto / revisado (cuándo se abrió a verlo, por quién).
  3. Procesado por el dispositivo (deviceID que lo tomó en su poll).
  4. Desenlace: Firmado (verde, con "PAdES + sello de tiempo") o Rechazado (rojo,
     con motivo).
- El contador (n) del tab = número de eventos registrados.

---

## 7. Secciones de CONFIGURACIÓN

- Mis dispositivos: lista de devices del usuario (alias, deviceID, estado, firmware,
  último heartbeat). Estado amigable: En línea / Dormido / Inhabilitado. Clic ->
  drawer con detalle del device (y acceso a sus preferencias).
- Mis preferencias: formulario de sign_prefs (ver 7.2 de DISENO_CONSOLE). Básicas
  visibles; avanzadas (imagen, opacidades, box, modo certify, borde) colapsadas.
  Se guarda por device. El TSA NO aparece (es del tenant).
- Mi consumo: saldo disponible del tenant y consumo del usuario (operaciones,
  por fechas). Lectura de billing.

---

## 8. Estados (mapeo amigable)

| Estado real | Bandeja | Cómo se ve |
|---|---|---|
| solicitado, sin procesar | Pendientes | fila normal + Firmar/Rechazar |
| solicitado, device dormido | Pendientes | badge "En cola" |
| firmado OK | Firmados | fecha verde + Descargar |
| rechazado por el usuario | Rechazados | fecha roja + motivo |
| error técnico | Pendientes (o Rechazados) | badge "Error" + reintentar (a decidir) |

NOTA de modelo (pendiente de confirmar y reflejar en BD):
- Origen de pendientes: subidos por el usuario y/o solicitados por una app externa.
- "Rechazado": decisión del usuario (con motivo). El "error técnico" se trata
  aparte (no es lo mismo que un rechazo). Si se confirma, añadir estado 'rechazado'
  y un campo motivo en sign_requests, separado de 'error'.

---

## 9. Datos y endpoints por pantalla (guardián)

| Pantalla | Lee de BD | Llama al optimizador |
|---|---|---|
| Bandejas | sign_requests (WHERE user_id) | GET jobs/{rid} (estado en vivo) |
| Nueva firma | devices, device_prefs | POST /devices/{id}/jobs (encolar) |
| Detalle/Documento | sign_requests + archivo | GET pdf/download (si firmado) |
| Trazabilidad | eventos del sign_request | — (eventos los registra console) |
| Mis dispositivos | devices (WHERE user_id) | GET /devices/{id} (estado) |
| Mis preferencias | device_prefs | — |
| Mi consumo | billing_events, tenants.saldo | — |

console valida SIEMPRE que el recurso pertenezca al usuario de la sesión antes de
cualquier acción (aislamiento por user_id + tenant_id).

NUEVO requerimiento de datos detectado (para la trazabilidad): se necesita una
tabla/lista de eventos por documento. Propuesta: tabla `sign_events` (id,
sign_request_id, tipo[enviado|abierto|procesado|firmado|rechazado], actor, ts,
meta json). Se añade al diseño de BD (deuda de modelado de Fase 1).

---

## 10. Mockups aprobados (referencia)

Mockups validados con el usuario (sesion 2026-06-11):
1. Inbox con header + sidebar colapsable (acordeones INBOX/CONFIGURACION).
2. Listado tipo email con columnas Documento/Tamano/Paginas/Fecha y fecha variable
   por bandeja (Solicitado/Firmado/Rechazado).
3. Drawer "Nueva firma": select de dispositivo, arrastrar PDF, motivo, enviar.
4. Drawer de detalle con tabs "Documento" (vista previa + Firmar/Rechazar) y
   "Trazabilidad (n)" (linea de tiempo: enviado -> abierto -> procesado ->
   firmado/rechazado).

La construccion debe respetar estos mockups. El branding fino se aplica sobre
esta estructura.

---

## 11. Orden de construccion del panel (Fase 1.E)

1. Estructura base: header + sidebar (colapsable, redimensionable, localStorage) + main vacio. Login que entra aqui.
2. Bandeja Pendientes (listado desde sign_requests) con datos de prueba.
3. Drawer de detalle: tab Documento (vista previa) + tab Trazabilidad.
4. Drawer Nueva firma: subir/encolar (guardian -> optimizador).
5. Bandejas Firmados / Rechazados + acciones (descargar / ver motivo).
6. Secciones Config: Mis dispositivos, Mis preferencias, Mi consumo.
7. Apurar cola (misma red) y Web Serial: subfase posterior (DT8).
