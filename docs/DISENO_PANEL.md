# Diseño del Panel del miniHSM (Xami-A1)

**Estado:** diseño cerrado, pendiente de implementar. Fecha: 2026-06-13.
**Resumen:** un mini sitio web responsivo servido por el propio chip que unifica
la configuración y operación del dispositivo (Wi-Fi, wallet, custodia de P12, ver
certificados, clave de acceso). Absorbe las páginas sueltas (`/custodia`) y elimina
de raíz el problema de mixed-content.

## 1. Concepto y por qué

El panel se sirve **desde el chip** y se opera en el **mismo origin http**, así que
puede hablar con `/device`, `/ceremony`, etc. sin que el navegador bloquee nada.

- **Mixed-content:** el navegador no deja que una página `https://` (xami.run) haga
  `fetch` a `http://192.168.1.41` (el chip). Sirviendo el panel desde el chip, todo
  es http mismo-origin y el problema desaparece sin TLS ni certificados. (Se descartó
  HTTPS en el chip por el lío del certificado de confianza.)
- Patrón técnico: igual que `/custodia` hoy — el chip devuelve un HTML mínimo que
  carga `panel.js`; el JS construye toda la UI y corre en el origin del chip.

## 2. Modos de red (STA / AP)

- **STA (estación):** el chip se une a tu Wi-Fi existente; tiene IP en tu red; entras
  al panel por esa IP. Es el modo de **uso normal** (y el único en que el chip opera:
  heartbeat, firmar, wallet con xami.run, NTP necesitan internet).
- **AP (punto de acceso):** el chip emite su propia red Wi-Fi; te conectas a ella
  (móvil) y el panel sale en una IP fija (p.ej. 192.168.4.1). Es para **bootstrap y
  rescate**, no para el día a día (el AP no da internet).

Flujo:
- Sin red configurada (primer arranque) -> el chip levanta el AP -> te conectas a su
  red -> metes tu Wi-Fi en el panel -> pasa a STA.
- Con red -> entras por IP cuando quieras y cambias lo que sea, incluida la Wi-Fi.
- Si se queda sin red (clave mal, router cambiado) -> vuelve a levantar el AP solo.

El AP se dispara de dos formas: **automático** (sin red al arrancar) y **a botón**
desde el panel ("modo configuración", para cambiar de red sin riesgo de quedar fuera).

**IP fija:** opción en la card de Wi-Fi (IP/gateway/máscara/DNS) además de DHCP, con
**fallback automático a DHCP** si tras X segundos no hay conectividad (evita perder el
chip por una IP mal puesta).

## 3. Autenticación y bloqueo

- **Clave de acceso (login):** la primera vez se crea; se guarda solo un **verificador**
  (sal + secreto del chip, nunca en claro). `POST /panel/login` emite un **token de
  sesión**. Distinta de la passphrase de las credenciales y de la clave Wi-Fi.
- **Doble auth en endpoints sensibles** (`/ceremony`, `/provision`, `/sign`, `/device`):
  se acepta el **token de API** (llamadas desde xami.run/server) **o** la **sesión del
  panel** (acceso local). Motivo: si abren puertos del router, no confiar solo en el NAT.
  `/health` queda público (liveness).
- En **modo AP**, el panel también exige el mismo login (o el AP con clave WPA2), para
  que un vecino en alcance Wi-Fi no reconfigure el chip.
- **Bloqueo (anti-fuerza-bruta):** tras N intentos el chip se autobloquea y, cuando
  tenga internet, **avisa al server** en su latido. El desbloqueo se hace desde
  xami.run (el server ya tiene el campo `blocked` en `sold_devices.json`). Sin internet:
  desbloqueador por USB (a definir después).

## 4. Las cards del panel

1. **Configurar Wi-Fi:** red + clave (envuelve `provision_wifi`), IP fija/DHCP, y el
   botón "modo configuración" (forzar AP). Avisar en UI que al cambiar de red "te mueves
   tú también" y el panel queda en la IP/red anterior hasta recargar en la nueva.
2. **Wallet:** crear (con verificación por reconocimiento), derivar address, ver address.
   Es el Paso 2 EVM (ver `DISENO_WALLET_EVM.md`).
3. **Agregar P12:** la ceremonia de custodia existente (`/ceremony` + ECIES), eligiendo
   modo **agente/autorización** (flag ya implementado en P1.2).
4. **Ver certificados:** como `/device` (ya desglosa cert: subject/issuer/serial/vigencia),
   con la opción de marcar un **predeterminado** (ver sección 5).
5. **Cambiar clave:** pide la actual y re-cifra el verificador.

## 5. Cert predeterminado / agente automatizado (decisión A)

Hoy un PDF encolado **sin** credencial se firma con la clave del vault (autofirmado).
Queremos que use el **cert predeterminado** marcado como agente. Regla: **solo una
credencial de modo agente** puede ser predeterminada (las de autorización exigen TOTP
por firma, no son desatendibles).

**Decisión clave (minimizar confianza en el chip):** el secreto del agente NO vive en
el chip. Es **xami.run quien inyecta el token/passphrase al encolar**:

- En la card 4 marcas el predeterminado y das su passphrase **una vez**; el panel se lo
  manda al **server** (API con token). El server la guarda cifrada de su lado.
- Cuando llega un PDF sin credencial, el server re-cifra la passphrase por ECIES hacia
  la pubkey del chip y encola el job con `credential_id` + `auth` ya puestos.
- El chip firma con lo que recibió. Si el server no tiene predeterminado: **fallback al
  cert del vault** (como hoy). El firmware del chip queda casi igual (ya soporta firmar
  por `credential_id` + `auth` desde P1.1–P1.3).

Consecuencia: el "predeterminado" es casi todo **server-side**; el chip no retiene
passphrases y se puede borrar sin problema.

## 6. Assets web (decisión D)

El `panel.js` debe servirse **desde el chip** (no de xami.run) para que un xami.run
comprometido no pueda inyectar JS que vea la clave del panel o la mnemónica.

- **Durante el desarrollo:** servir `panel.js` desde xami.run (como custodia hoy) para
  iterar la UI rápido sin recompilar firmware.
- **Al endurecer (Fase 6):** mover los assets al chip. Dos opciones: embebidos en el
  firmware (simple, infla el binario, recompila para tocar UI) o en una **partición
  LittleFS** (más limpio y editable). Agregar partición **obliga a erase-flash ->
  nueva identidad -> re-ceremonia**, así que conviene planear **un solo erase** que
  reacomode todo (assets del panel + audit B6) y no quemar la identidad dos veces.

## 7. Plan de implementación (fases pequeñas, de la base hacia arriba)

- **Fase 0 — Esqueleto:** handler `GET /panel` (HTML mínimo que carga `panel.js`) +
  `panel.js` con layout responsivo y las 5 cards como placeholders. Validar que carga
  y lee `/device`. Sin login todavía.
- **Fase 1 — Login y sesión (B):** verificador de clave en NVS, `POST /panel/login` con
  token, middleware de doble auth (token API o sesión) en endpoints sensibles, card 5
  (cambiar clave), autobloqueo básico + aviso al server (C).
- **Fase 2 — Card Wi-Fi + IP fija (card 1):** UI sobre `provision_wifi`, IP fija con
  fallback DHCP, botón "modo configuración" (AP) + AP automático sin red. Revisar qué
  hay ya hecho de AP/provisioning (`handler_provision_reconfigure` parece reutilizable).
- **Fase 3 — Card Ver certificados + predeterminado (card 4):** lee `/device`; marcar
  predeterminado se manda al server (la A). Trabajo principalmente server-side.
- **Fase 4 — Card Agregar P12 (card 3):** integrar la ceremonia en el panel con modo.
- **Fase 5 — Card Wallet (card 2):** Paso 2 EVM completo (ya hay T1 y T2.1; falta el
  resto: BIP-39/32, crear+verificar por reconocimiento, derivar, listar, importar, firmar).
- **Fase 6 — Endurecimiento:** mover `panel.js` al chip (D), planear el erase junto con
  audit B6, pulir bloqueo/desbloqueo, rescate por USB.

Nota: con este orden, la **firma de PDF queda dirigida por xami.run** (el server encola)
y el panel se dedica a **configurar** el dispositivo. Eso hace que el problema original
de `/firmar` + mixed-content **desaparezca**: ya no hace falta que `/firmar` lea
credenciales del chip, porque el predeterminado lo configura el panel y vive en el server.

## 8. Decisiones pendientes para arrancar

- Confirmar servir `panel.js` desde xami.run durante el desarrollo y moverlo al chip en
  Fase 6 (recomendado).
- Confirmar el orden de fases (¿wallet antes que las cards de configuración?).
- Reusar vs rehacer el AP/provisioning existente (revisar en Fase 2).

## 9. Relación con lo ya hecho

- `/device` ya expone credenciales + cert desglosado + modo (P1.1, P1.1b, P1.2).
- Firma por `credential_id` + `auth` (ECIES) + TOTP condicional al modo (P1.3) ya operativa.
- Cripto ECIES del cliente reutilizable: `optimizer/web/firmar-custodia.js` y
  `custodia/app.js` (forge + elliptic, `eciesEncrypt`).
- La web `/firmar` con selector (P1.4) queda como demo; el panel la deja de necesitar.
