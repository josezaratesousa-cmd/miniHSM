# Análisis — Panel de USUARIO (dueño del deviceID)

> Especificación funcional del panel del usuario final que opera un deviceID.
> Fase 1 del PLAN_DE_TRABAJO_CONSOLE. Estado: ANÁLISIS.
> Acompaña a DISENO_CONSOLE.md (4.3) y BD/DISENO_BD.md.

---

## 1. Objetivo

Dar al dueño de uno o varios deviceIDs una interfaz simple para FIRMAR documentos
y gestionar sus dispositivos, ocultando la complejidad técnica. El usuario es
funcional (no técnico): debe poder firmar sin entender criptografía, NAT ni colas.

## 2. Quién lo usa

- Un USUARIO dueño de 1..N devices, perteneciente a 1 tenant.
- Ya autenticado (login se cubre en 1.E.1). El panel deriva tenant_id+user_id de
  la sesión; nunca de parámetros del cliente.
- Un usuario que además sea gestor verá un acceso extra al panel de gestión, pero
  sus devices propios los opera desde ESTE panel.

## 3. Alcance (qué entra y qué no)

ENTRA en Fase 1:
- Ver mis devices y su estado.
- Configurar preferencias de firma reutilizables.
- Enviar a firmar un PDF y ver el resultado.
- Ver la cola/estado de las firmas.
- Apurar la cola (solo misma red) — puede quedar parcial.
- Log por Web Serial (solo misma red) — puede quedar para subfase aparte.

NO entra en Fase 1 (otras fases / deuda):
- Alta/baja de devices (eso es del gestor o super-admin).
- Ceremonia CA / emisión de certificado (fase final).
- Rotación de secretos (panel gestor).

---

## 4. Funcionalidades

### F1. Mis dispositivos
- Lista de los devices del usuario (de la BD: devices WHERE user_id = sesión).
- Por cada device: alias/deviceID, estado (activo/inhabilitado), y estado de
  conexión leído del optimizador (despierto/dormido, último heartbeat, firmware).
- Indicador claro y amigable: "En línea" / "Dormido" / "Inhabilitado".
- Seleccionar un device entra a su detalle (preferencias, firmar, cola).

### F2. Preferencias de firma (reutilizables)
- Formulario que edita `device_prefs.sign_prefs` (JSON).
- Campos (de DISENO_CONSOLE 7.2): name, reason, location, contact; apariencia del
  sello (visible, page, box/posición), texto (stamp_text, stamp_source), imagen
  (stamp_image, image_mode, image_width, image_opacity), text_opacity, borde
  (border, border_width), modo (approval/certify).
- El TSA NO se configura aquí (es del tenant).
- Se guarda una vez y se REUTILIZA en cada firma. Vista previa deseable (futuro).
- Avanzado (mode certify, opacidades, box exacto) va en sección "Opciones
  avanzadas" colapsada para no abrumar.

### F3. Enviar a firmar
- Subir un PDF.
- Usa las preferencias guardadas por defecto; permite overrides puntuales.
- console (guardián) valida: device del usuario, activo, saldo del tenant > 0;
  luego encola en el optimizador (POST /devices/{id}/jobs) y registra en
  sign_requests.
- Devuelve un request_id y muestra estado "en cola".

### F4. Cola / estado de firmas
- Lista de solicitudes del usuario con su estado (pendiente, entregado, firmado,
  error) leyendo sign_requests + estado del job en el optimizador.
- Al quedar "firmado": botón de descarga del PDF firmado.

### F5. Apurar la cola (solo misma red)
- Botón "Firmar ahora" que pide al device un poll inmediato.
- ACTIVO solo si el navegador del usuario está en la misma red que el device
  (si no, deshabilitado con tooltip explicativo). Ver regla R8 / DT8.

### F6. Log del device por Web Serial (solo misma red / USB)
- Ver el log del device conectado por USB, con UI amigable (no el serial-terminal
  crudo). Para diagnóstico. Ver DT8.
- Opción técnica/avanzada, no principal.

---

## 5. Estados que ve el usuario (mapeo amigable)

| Estado real (BD/optimizador) | Cómo se muestra |
|---|---|
| device activo + heartbeat reciente | "En línea" (verde) |
| device activo + sin heartbeat reciente | "Dormido" (gris) |
| device inhabilitado | "Inhabilitado" (rojo) |
| job pendiente/entregado | "En cola" |
| job firmado | "Firmado ✓" + descarga |
| job error | "Error" + reintentar |

---

## 6. Reglas de negocio del panel

- RU1. El usuario solo ve y opera SUS devices (user_id = sesión). Aislamiento
  estricto; nunca ver devices de otros usuarios ni de otro tenant.
- RU2. No se puede firmar si: el device está inhabilitado, no pertenece al
  usuario, o el saldo del tenant es 0. Mensaje claro en cada caso.
- RU3. Las preferencias son por device (cada device su identidad/apariencia).
- RU4. "Apurar" y "Web Serial" solo se habilitan en misma red (R8).
- RU5. Cada firma facturable descuenta saldo (registrar en billing_events). La
  regla exacta de billing está pendiente (DT6); en Fase 1 se registra el consumo.
- RU6. Las opciones avanzadas están colapsadas por defecto.

## 7. Casos borde

- Usuario sin devices: pantalla vacía con mensaje ("aún no tienes dispositivos
  asignados, contacta a tu gestor").
- Device dormido: se puede encolar igual; se firmará cuando el device despierte
  en su próximo poll. Avisar que puede tardar.
- Saldo 0: bloquear envío con mensaje; sugerir contactar al gestor.
- PDF inválido o muy grande: validar y mostrar error antes de encolar.
- Pérdida de conexión al optimizador: mostrar estado "no disponible", no romper.

## 8. Criterios de aceptación (prueba de Fase 1)

1. El usuario entra con demo@xami.run y ve el device fe4dfede3b10c54b "En línea".
2. Edita sus preferencias de firma y se guardan (persisten al recargar).
3. Sube un PDF, lo envía a firmar, ve "En cola" y luego "Firmado".
4. Descarga el PDF firmado y el sello aparece según sus preferencias.
5. El consumo queda registrado (saldo/billing refleja la operación).
6. Si el device estuviera inhabilitado o saldo 0, el envío se bloquea con mensaje.

## 9. Dependencias y notas

- Depende de Fase 0 (BD + datos de prueba + endpoints + conexión). HECHA.
- El flujo de firma real depende del polling (Bloque 10, ya implementado por el
  otro asistente en firmware/optimizador). console solo encola y consulta.
- "Misma red" (RU4) y Web Serial (F6) tienen incógnitas técnicas: ver DT8. Pueden
  entregarse como subfase posterior sin bloquear el resto del panel.
