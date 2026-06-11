# PLAN DE TRABAJO — Console (panel de gestión de Xami)

> Plan por fases y tareas cortas, probables por el usuario. Acompaña a
> `docs/DISENO_CONSOLE.md` (qué/por qué) y `docs/BD/DISENO_BD.md` (modelo de datos).
> Estado: EN CURSO. Fecha inicio: 2026-06-11.
>
> Regla de oro: NO se toca firmware ni optimizador (api.xami.run) salvo lo
> marcado como deuda técnica, y siempre coordinado. console se construye aparte.

---

## Metodología

Cada panel se construye con este ciclo de documentos y aprobaciones:

1. `analisis/`   — especificación funcional del panel (qué hace, para quién, reglas).
2. Mockup        — diseño visual navegable.
3. Aprobación    — el usuario aprueba el mockup antes de seguir.
4. `diseno/`     — especificación de diseño (pantallas, componentes, flujos, datos).
5. `desarrollo/` — documentación técnica para la IA: contexto, cómo está hecho,
   deudas técnicas de TODO console, consideraciones de seguridad, convenciones.
   (Pueden ser varios documentos.)
6. Construcción  — se codea el panel.
7. `pruebas/`    — el usuario prueba; se registran resultados.
8. `produccion/` — notas de despliegue / lo que queda en producción.

Tareas CORTAS y verificables por el usuario en cada paso. No se avanza de fase
sin que lo previo esté probado/aprobado.

---

## Prioridad de construcción (orden inverso a la jerarquía)

Se construye primero lo que da valor al usuario final, luego hacia arriba:

1. PRIMERO — Panel de USUARIO (dueño del deviceID): el que firma.
2. LUEGO   — Panel del GESTOR del tenant: administración.
3. FINAL   — Panel del CREADOR de tenant (super-admin xami.run).

Razón: el usuario ya tiene un deviceID conectado y emparejado; podemos probar el
flujo de firma real de punta a punta desde su panel cuanto antes.

---

## FASE 0 — Cimientos (BD + endpoints + estructura)

Objetivo: dejar la base lista para construir paneles encima.

- [ ] 0.1 Crear la estructura de carpetas de documentación:
  `/docs/console/analisis`, `/diseno`, `/desarrollo`, `/produccion`, `/pruebas`. (HECHO)
- [ ] 0.2 Crear las tablas en `xami_db` según `docs/BD/DISENO_BD.md` (8 tablas,
  InnoDB, utf8mb4). Script SQL versionado.
- [ ] 0.3 Cargar datos por defecto para pruebas: 1 tenant de prueba, 1 usuario
  dueño, el deviceID real ya conectado (`fe4dfede3b10c54b`) asignado a ese usuario,
  un saldo de firmas. (Para poder probar el panel de usuario de inmediato.)
- [ ] 0.4 Definir los endpoints (contrato) de cada panel — documento en
  `/desarrollo`. Separar: endpoints que console expone (PHP) vs endpoints del
  optimizador que console consume (guardián). Tabla método/ruta/entrada/salida/rol.
- [ ] 0.5 Verificar conexión PHP<->BD desde console (lectura robusta del `.env`
  con `~` en el password) y un "hola BD" que liste las tablas.

Prueba de Fase 0 (usuario): ver las 8 tablas creadas en phpMyAdmin/cPanel y los
datos de prueba cargados (el tenant, el usuario, el device asignado).

---

## FASE 1 — Panel de USUARIO (dueño del deviceID)

Contexto: ya hay un deviceID conectado y emparejado (`fe4dfede3b10c54b`). Debe
quedar matriculado para el usuario de prueba (Fase 0.3), aún sin login formal,
para poder probar el panel cuanto antes.

### 1.A Análisis
- [ ] 1.A.1 Documento de especificaciones funcionales del panel de usuario en
  `/docs/console/analisis/PANEL_USUARIO.md`: qué ve y hace el usuario (mis devices,
  preferencias de firma reutilizables, enviar a firmar, ver cola, apurar cola en
  misma red, log por Web Serial en misma red, avanzado oculto).

### 1.B Mockup
- [ ] 1.B.1 Diseño de mockup navegable del panel de usuario.
- [ ] 1.B.2 Aprobación del mockup por el usuario. (NO se sigue sin aprobación.)

### 1.C Diseño
- [ ] 1.C.1 Documento de diseño en `/docs/console/diseno/PANEL_USUARIO.md`:
  pantallas, componentes, flujos, qué datos usa de la BD, qué endpoints llama.

### 1.D Desarrollo (documentación técnica para la IA)
- [ ] 1.D.1 En `/docs/console/desarrollo/`: contexto del proyecto console, cómo
  está desarrollada la solución, convenciones (PHP modular, lectura .env, PDO,
  guardián frente al optimizador), estructura de carpetas y archivos.
- [ ] 1.D.2 Lista consolidada de DEUDAS TÉCNICAS de todo console (referencia a las
  DT de DISENO_CONSOLE + nuevas que surjan).
- [ ] 1.D.3 Consideraciones de seguridad de desarrollo (sesiones, CSRF, validación,
  aislamiento por tenant, manejo de secretos). Pueden ser varios documentos.

### 1.E Construcción
- [ ] 1.E.1 Login del usuario (sobre el index.php ya existente en /console).
- [ ] 1.E.2 Vista "mis devices" (lista + estado de polling leído del optimizador).
- [ ] 1.E.3 Preferencias de firma reutilizables (form que guarda sign_prefs).
- [ ] 1.E.4 Enviar a firmar (subir PDF -> guardián -> encola en optimizador).
- [ ] 1.E.5 Ver cola / estado de la firma (consulta el job).
- [ ] 1.E.6 Apurar cola (botón activo solo en misma red) — puede quedar parcial.
- [ ] 1.E.7 Log por Web Serial (UI amigable) — puede quedar para 1.F o fase aparte.

### 1.F Pruebas
- [ ] 1.F.1 El usuario prueba: entra, ve su device, configura prefs, firma un PDF
  real de punta a punta, ve el resultado. Registrar en `/docs/console/pruebas/`.

Prueba de Fase 1 (usuario): firmar un documento real desde el panel y descargarlo
firmado, con las preferencias aplicadas.

---

## FASE 2 — Panel del GESTOR del tenant

Mismo ciclo que la Fase 1 (analisis -> mockup -> aprobacion -> diseno ->
desarrollo -> construccion -> pruebas), con documentos en las carpetas
correspondientes (analisis/PANEL_GESTOR.md, diseno/PANEL_GESTOR.md, etc.).

Alcance funcional (de DISENO_CONSOLE 4.2):
- [ ] 2.1 Analisis + mockup + aprobacion + diseno.
- [ ] 2.2 Dashboard: inventario de deviceIDs, operaciones (billing), saldo,
  actividad por dias (graficos), logs recientes para soporte.
- [ ] 2.3 Monitoreo de devices: estado del polling (despierto/dormido, sincronizado).
- [ ] 2.4 Gestion de usuarios: crear usuario con clave temporal, capturar datos de
  certificado + identidad CA (cedula, tipo doc). Captura ya; ceremonia CA al final.
- [ ] 2.5 Gestion de devices: asignar device a usuario, rotar secretos
  (STAMPING_API_KEY y otros), configurar personalizable, desactivar (inhabilitar).
- [ ] 2.6 Pruebas del gestor.

Nota: el rol gestor es un flag sobre el usuario; el mismo login ve su panel de
usuario y, si es gestor, ademas el de gestion.

---

## FASE 3 — Panel del CREADOR de tenant (super-admin xami.run)

Mismo ciclo completo, documentos en las carpetas correspondientes.

Alcance funcional (de DISENO_CONSOLE 4.1):
- [ ] 3.1 Analisis + mockup + aprobacion + diseno.
- [ ] 3.2 Crear tenant (codigo + clave temporal) y onboarding con link OTP 48h por
  email; reemitir link si caduca.
- [ ] 3.3 Tabla de devices HUERFANOS + asignacion de deviceID a un tenant.
- [ ] 3.4 Asignar saldo de firmas (billing).
- [ ] 3.5 Configurar CA y parametros generales del tenant (incluye TSA en ca_config).
- [ ] 3.6 Pruebas del super-admin.

---

## FASE FINAL — Integracion con el CA (coordinada, toca firmware/optimizador)

Se aborda al final, requiere coordinacion externa con el CA. Ver DT1, DT2, DT3,
DT10, DT11 de DISENO_CONSOLE.

- [ ] F.1 Captura/validacion de datos de identidad segun exija el CA.
- [ ] F.2 Ceremonia con el CA y emision del certificado real.
- [ ] F.3 Cambiar el match para que el device quede HUERFANO sin credencial hasta
  tener datos + CA (TOCA firmware + optimizador).
- [ ] F.4 El optimizador respeta el estado "inhabilitado" en el encolamiento
  (TOCA optimizador).

---

## Convenciones del plan
- Tareas `[ ]` pendientes, `[x]` hechas. Marcar HECHO en el texto cuando aplique.
- No avanzar de subfase sin prueba/aprobacion del usuario.
- Cada panel deja su rastro documental en analisis/diseno/desarrollo/pruebas.
- Las deudas tecnicas vivas se consolidan en /docs/console/desarrollo/.
