# Diseño — Servicio de custodia de certificados (.p12) en el miniHSM

> Estado: DISEÑO CERRADO (ceremonia, cifrado, firma, anti-replay) + análisis de
> impacto multi-credencial. Pendiente de implementar.
> Fecha: 2026-06-12

## 1. Motivación y modelo
El miniHSM hoy genera su propia clave (la identidad del dispositivo). Pero un usuario
quiere firmar con un **certificado que YA existe** (emitido por una CA, bajo su
responsabilidad). Para eso se añade un servicio que **carga un .p12 (cert + clave
privada) en custodia dentro del chip**, y el chip firma con esa clave.

No se reemplaza la clave de iniciación: se **AGREGA**. El chip tiene dos roles de clave:
- **Clave de iniciación** -> identidad del *dispositivo* (sostiene match/heartbeat con xami.run).
- **Clave(s) custodiada(s)** -> identidad de la *persona/dueño* (firma de documentos).

Esto rompe a propósito el lema "la clave nunca entra/sale del chip": es una decisión
consciente para que el equipo sea **custodio de un certificado existente, bajo
responsabilidad del usuario** (similar a cargar un cert en un token/HSM).

**Aclaración clave — la clave de iniciación (device) es ÚNICA por chip y COMÚN a todas las
credenciales.** Su rol es **transporte e identidad**, no firmar documentos: los clientes
cifran hacia su pubkey (ECIES) para que **solo el chip** abra los paquetes (.p12, autorización
de firma), y sirve para el **match** con xami.run y para el **fingerprint**. La firma de
documentos la hacen las **claves custodiadas**, cada una con su passphrase + TOTP. Que el
transporte use una clave compartida **no rompe la separación por usuario**: la autorización
real es **por credencial** (passphrase + TOTP individuales).

## 2. Principios transversales
- Todo lo sensible (carga del .p12, enrolamiento TOTP, autorización de firma) ocurre
  **solo en la red WiFi local**, contra el servidor HTTP del propio chip. **Nunca desde xami.run.**
- **xami.run = cartero ciego**: orquesta pantallas y transporta blobs opacos. Nunca ve
  la clave del .p12, ni la passphrase, ni la semilla TOTP, y no puede firmar por su cuenta.
- **Aislamiento físico** para la ceremonia: el chip levanta su propio WiFi (AP) y el
  usuario se conecta ahí -> no hay internet de por medio (hecho físico, no promesa).
- **Confianza anclada en la pubkey del chip**, verificada por *fingerprint* ANTES de cortar internet.

## 3. Cifrado en reposo de la clave custodiada
- `KEK = HKDF( passphrase_usuario || chip_secret || salt )`
- `priv_custodiada_en_NVS = AES-256-GCM( KEK, priv )`, guardando al lado nonce + tag + salt.
- El **cert va en claro** (no es secreto).
- **chip_secret** (lo que aporta el equipo, que xami.run NO conoce):
  - **MVP (Opción 1):** clave aleatoria propia del chip en NVS. Se encapsula en UNA
    función `vault_get_chip_kek_secret()` para poder migrar luego sin tocar el resto.
  - **Final (Opción 2):** derivada de **eFuse + periférico HMAC** del ESP32-S3 + **flash
    encryption**. No vive en flash -> resiste un volcado físico. Solo cambia esa función.
  - **Migración 1 -> 2:** toca solo esa función, PERO obliga a **re-enrolar** (la
    passphrase no se guarda, así que el .p12 no se puede re-cifrar solo).
- Activar flash encryption **NO congela el firmware**: OTA sigue funcionando; lo que
  cambia es el flasheo por cable. El eFuse es irreversible (solo esa clave), no el código.

## 4. Passphrase
- El usuario elige una; el sistema **exige que sea fuerte** (medidor de entropía + rechazo
  de débiles) y ofrece un **generador de claves duras aleatorias**.
- **No se guarda** en el chip; se pide **en cada firma**.
- Roles: **passphrase = confidencialidad** (cifra el .p12) **+ recuperación/rotación**.
  **TOTP = uso diario / autorización.**
- **AVISO al usuario:** debe **recordar/guardar** la passphrase, porque la necesitará si:
  1. Pierde o cambia el celular (pierde la semilla TOTP).
  2. Quiere **rotar** su secreto de Google Authenticator (re-enrolar un autenticador nuevo).
- En el día a día no la teclea (accede por TOTP), pero es su **llave maestra de
  recuperación**. Sin passphrase y sin TOTP -> no hay acceso al .p12.

## 5. Enrolamiento de Google Authenticator (TOTP)
- El chip genera una **semilla TOTP** aleatoria; vive **solo en su NVS**. Nunca pasa por xami.run.
- Muestra el **QR (`otpauth://`)** en la UI local (servida por el chip); el usuario la escanea.
- Pide un **primer código** para confirmar sincronización (semilla + hora).
- El chip **valida el TOTP localmente**, contra su semilla.
- **Dependencia dura:** TOTP requiere **hora sincronizada -> NTP en firmware** (Bloque 0 del
  plan, hoy pendiente). Es prerrequisito antes de implementar TOTP.

## 6. Ceremonia de carga del .p12 (flujo seguro; el orden importa)
1. xami.run ordena al chip "inicia enrolamiento" y le **entrega un secreto de ceremonia**
   (token de **un solo uso**, con caducidad).
2. **Con internet todavía:** xami.run entrega al JS del usuario (a) la **pubkey legítima**
   del chip, (b) su **fingerprint**, y (c) el mismo **secreto de ceremonia**.
3. El chip **se desconecta de internet** y levanta el **WiFi de transferencia (AP)**. Queda
   esperando el paquete de ceremonia con ese secreto.
4. El usuario se une al WiFi del chip (ya **fuera de internet**). La UI la sirve el chip.
5. El chip muestra **su** fingerprint. El **JavaScript compara** (chip vs el de xami.run);
   si **no coinciden, bloquea el avance** (el usuario solo confirma una decisión binaria).
   Impostor -> fingerprint distinto -> no avanza.
6. El usuario sube el **.p12** + define **passphrase fuerte**. El JS **cifra TODO el paquete
   (ECIES) hacia la pubkey verificada del chip** e **incluye el secreto de ceremonia**.
7. El chip **descifra** (solo él puede), **verifica que el secreto coincide** con el que
   xami.run anunció, abre el .p12 (cert+priv), **valida** (priv<->cert, vigencia), deriva la
   **KEK**, cifra la priv y la guarda en NVS. Genera/guarda la **semilla TOTP** y muestra el QR.
   **Zeroiza** priv y passphrase.
8. Fin de ceremonia -> el chip **vuelve a su WiFi normal** (timeout / botón / reinicio).

**Capas de seguridad resultantes:** aislamiento físico (AP) + identidad verificada
(fingerprint comparado por JS, anclado en xami.run antes del corte) + confidencialidad
(ECIES a la pubkey del chip) + autorización probada (secreto de ceremonia de un solo uso:
la ceremonia solo procede si xami.run la inició, el usuario lo trajo y el chip lo esperaba).

## 7. Firma individual y masiva (por lote)
- La firma **individual** es el caso **N = 1** del lote.
- `lote_id = raíz de Merkle` del lote (hojas = SHA-256 de cada PDF).
- Flujo:
  1. El usuario arma el lote de N documentos. Se calcula el **Merkle root**.
  2. Pantalla: muestra los **documentos** + acordeón discreto **"Huella del lote"** (el root,
     el algoritmo, y opcionalmente las hojas). Pide **TOTP + passphrase**.
  3. El JS **cifra `{ lote_id, TOTP, passphrase, nonce_del_chip }` (ECIES)** hacia la pubkey del chip.
  4. xami.run transporta el **blob opaco** + los documentos al chip (cartero ciego).
  5. El chip: descifra -> **re-calcula el Merkle root** y compara con `lote_id` -> **valida el
     TOTP** contra su semilla -> **valida frescura** (nonce) -> deriva la **KEK** con la
     passphrase -> **descifra la priv custodiada** -> firma los **N** -> **zeroiza**.
  6. Devuelve los N documentos firmados.
- **Atestación en blockchain:** el Merkle root se atesta -> **prueba de autorización +
  sellado de tiempo** del lote. En el campo `info` se envía la **estructura del Merkle tree**
  (hojas/ramas) para reconstruir y verificar el root. Permite **prueba de inclusión** de un
  documento sin revelar los demás. (Pendiente: qué cadena; publicar **solo el root**.)
- **Transparencia / auditoría:** el acordeón "Huella del lote" deja que cualquiera
  **recalcule y verifique** que se firmó ese conjunto exacto. Mecanismo declarado de seguridad.

## 8. Anti-replay (repetición)
- **Nonce-challenge de un solo uso emitido por el chip:** antes de firmar, el cliente pide
  un nonce; el chip lo guarda como pendiente (caduca rápido); va dentro del blob. Reenviar el
  blob -> nonce ya consumido -> **rechazado**. No depende del reloj.
- **TOTP de un solo uso:** el chip registra la ventana TOTP usada; no acepta el mismo código dos veces.
- **Atadura al lote:** el nonce va dentro del hash autorizado -> sirve para *ese* lote, no otro.

## 9. Multi-credencial — el chip como HSM de empresa (análisis de impacto)
**Visión:** un mismo chip custodia **varias credenciales** (una por empleado). Pasa a ser el
**HSM de una pequeña empresa**: guarda físicamente las claves de todos, pero **cada persona
solo puede usar la suya**. Impacto sobre el firmware actual (hoy es single-identity rígido):

a) **Almacenamiento indexado (vault + cert).** Hoy NVS usa keys singleton fijas
   (`vault`:`privkey`/`pubkey`, `cert`:`cert_pem`/`state`). Para N credenciales hay que
   **indexar**: por credencial `cust_<id>_priv` (cifrada), `cust_<id>_cert` (PEM),
   `cust_<id>_totp` (semilla), `cust_<id>_meta` (alias/dueño, fingerprint, vigencia,
   salt+nonce+tag) y un **registro/índice** (lista de IDs+alias). La clave de iniciación
   (`vault`) queda **intacta y separada**.

b) **Selector de credencial en el job de firma.** Hoy el job `{requestId, digest, kuser,
   ts, nonce}` **no dice con qué clave firmar** (siempre la del device). Hay que añadir
   `credentialId` (o alias). `process_signing_job` debe leerlo, firmar con ESA credencial y
   devolver **su** cert (no el del device).

c) **`vault_sign` -> `vault_sign_with(cred_id, ...)`.** Nueva función: carga la priv custodiada
   de `cred_id`, deriva la KEK (con la passphrase+TOTP que llegan autorizados), descifra,
   firma, zeroiza. Firmar deja de ser "gratis" (clave del device): exige autorización por credencial.

d) **`cert_manager` NO sirve tal cual para las custodiadas.** `cert_load_ca_signed` exige que
   la pubkey del cert == pubkey del device. Las custodiadas tienen la pubkey del **usuario**,
   no la del device -> esa validación no aplica. `cert_manager` se queda para la identidad
   propia del device; las custodiadas se gestionan aparte (en vault o un nuevo `custody_manager`).

e) **Control de acceso por credencial (lo potente).** Cada credencial tiene **su** passphrase
   y **su** semilla TOTP. -> El empleado X solo firma con la credencial de X (sabe su passphrase
   y tiene su autenticador). El chip guarda las claves de todos, pero la **autorización es
   individual**: nadie firma por otro, y xami.run no conoce ninguna passphrase ni semilla.
   Esto convierte el chip en un **HSM multi-usuario con separación real de identidades**.

f) **Selección "firmar como...".** El server y la UI (/firmar, consola) deben **listar las
   credenciales** (por alias/dueño) y dejar elegir "firmar como Empleado X". El job lleva ese
   `credentialId`. Cada **ceremonia de carga** añade un empleado.

g) **Identidad y match intactos.** El device mantiene **una** identidad (una clave, un
   deviceID, un secreto de match con xami.run). Las custodiadas son "inquilinos" encima.

h) **Límite real = espacio en flash.** Cada credencial ~ priv(32B cifrada) + cert PEM(~1-2 KB)
   + semilla TOTP + metadata. El driver de espacio es el **PEM del cert**. Hay que **dimensionar
   la partición NVS** (`partitions.csv`) según cuántos empleados soportar (decenas son viables).

i) **DECISIÓN (2026-06-12): N credenciales de custodia.** El equipo es un **chip de
   custodia** multi-usuario: **1 clave de device** (identidad/transporte) + **N custodiadas**
   (una por empleado). Supera la idea previa de "máximo 1 custodiada". El límite efectivo lo
   pone el espacio en flash (ver h).

## 10. Puntos abiertos / pendientes
- **Descubrimiento del chip en la LAN** (mDNS / IP): el anclaje de confianza va por
  *fingerprint*, así que el descubrimiento puede ser inseguro sin comprometer la seguridad.
- **Retorno del chip a su WiFi normal** tras la ceremonia (timeout / botón / reinicio).
- **Protección de la semilla TOTP en NVS** (cifrarla también, no dejarla en claro).
- **Orden exacto de las hojas** del Merkle tree (determinismo para reconstruir el root).
- **Qué blockchain** para la atestación; publicar **solo el root**, nunca contenido.
- **Dimensionar la partición NVS** para N credenciales.
- **NTP (Bloque 0)** como prerrequisito de TOTP.
- **Passphrase de custodia**: ¿reutilizar la del propio .p12 o pedir una separada? (cerrado:
  el usuario elige una fuerte; falta decidir si coincide con la del .p12).
- **Límite de credenciales** por chip (sección 9.i).

## 11. Estado actual del firmware (base v1.0.0) — referencia
- **Clave:** PSA Crypto (Mbed TLS) en el ESP32-S3, **NO ATECC608A**. ECC P-256. Exportable.
  Guardada en NVS (namespace `vault`, keys `privkey`/`pubkey`) **en claro** (sin flash
  encryption ni NVS cifrada; `main.c` usa `nvs_flash_init()` normal).
- **Cert:** namespace `cert` (`cert_pem`/`state`), **un** cert ligado a la clave del device.
  `cert_load_ca_signed` valida que la pubkey del cert == pubkey del device.
- **Match:** namespace `policy` (`hmac_secret`), **un** secreto device<->xami.run
  (ECDH + HKDF-SHA256 `info="xami-match-v1"` + AES-GCM; patrón ECIES en `match_engine.c`).
- **Firma:** heartbeat `process_signing_job` valida el token (kuser/HMAC) y llama
  `vault_sign(digest)` (clave **única**) -> devuelve sig + cert del device. **Sin selector
  de credencial.** Stack del heartbeat ya ampliado para firmar inline.
- **Arranque (`main.c`):** nvs -> crypto -> vault (genera/lee la clave) -> cert -> policy ->
  audit -> wifi; si no hay WiFi -> portal cautivo (AP); si hay -> http server + match + heartbeat.
