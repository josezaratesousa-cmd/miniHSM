# Agente automatizado — cifrado y custodia de la llave (Xami-A1)

**Estado:** diseño cerrado, en implementacion. Fecha: 2026-06-13.
**Idea:** una credencial marcada "agente" firma desatendida (sin passphrase ni TOTP),
pero el secreto para abrir su clave privada **no vive completo en ningun lado**: se
parte entre el chip (Kmaster) y xami.run (R). Hacen falta los dos para usarla.

## 1. Secretos en juego

- **Kmaster = `chip_kek_secret`** (NVS `kek_secret`, 32 B aleatorios, generado una vez,
  persistente, unico por dispositivo, nunca sale del chip). Ya se usa para la KEK de custodia.
- **R** = random de 32 B que **genera el chip** por credencial agente y **custodia xami.run**.
- **fingerprint** = SHA-256 del cert (publico), ata la llave a esa credencial concreta.

La llave que realmente cifra la privada del cert:

    K = cc_kek_derive(ikm = R, chip_secret = Kmaster, salt = fingerprint)

(es el mismo `cc_kek_derive` de hoy; en modo agente, R ocupa el lugar de la passphrase
y el fingerprint entra como salt). AES-256-GCM con K cifra/descifra la privada.

## 2. En la ceremonia (al agregar el cert como agente)

1. El chip genera R (32 B aleatorios).
2. Deriva K = cc_kek_derive(R, Kmaster, fingerprint) y **cifra la privada con K**.
3. Guarda el blob cifrado (NVS custody, como hoy). **No** enrola passphrase de usuario ni TOTP.
4. **Retiene R** en NVS marcado "pendiente de entrega" (NO lo olvida todavia).

## 3. Entrega de R a xami.run (estados)

- Con conexion, el chip envia a xami.run: `{device_id, fingerprint, R}` por el canal
  autenticado (token de API). Opcional E2E: cifrar R hacia una pubkey publicada por xami.run.
- xami.run guarda R y responde ACK. Estados que lleva xami.run:
  **Generada** (recibida, sin confirmar) -> **Aceptada** (custodiada).
- Al recibir el ACK "Aceptada", el chip **borra R** de su NVS (se olvida).
- Sin ACK: el chip **retiene R y reintenta**. Asi nunca se pierde la llave por un fallo de red.
- xami.run **no encola** firmas de esa credencial hasta que este en estado Aceptada.

Importante: si el chip se borra (erase-flash / nueva identidad), Kmaster cambia y la
credencial agente muere aunque xami.run tenga R. Es el comportamiento deseado.

## 4. Firma (job en la cola)

xami.run encola: `{ fingerprint, hash|pdf, nonce, payload }` donde
`payload = ECIES_hacia_pubkey_chip( {R, nonce} )` (cifrado punta a punta hacia el chip).

El chip:
1. Descifra `payload` con su privada -> obtiene `{R, nonce}`.
2. **Valida el nonce** contra el ultimo visto de esa credencial (NVS). Rechaza <=. Anti-replay.
3. Resuelve el **slot por fingerprint** (busca la credencial con ese fingerprint).
4. Deriva `K = cc_kek_derive(R, Kmaster, fingerprint)` y descifra la privada.
5. Firma. Actualiza el ultimo nonce. **Sin passphrase, sin TOTP.**

Capas (no confundir): el **fingerprint** liga la llave a la credencial (constante);
el **nonce** da frescura por firma (un solo uso). Mejora opcional: meter el hash del
documento como AAD del cifrado, para que esa entrega solo sirva para ese documento.

## 5. Modelo de amenazas

- xami.run comprometido (tiene R): sin Kmaster no deriva K -> no exfiltra la privada.
- Chip robado (Kmaster + privada cifrada): sin R no deriva K -> no firma.
- Solo descifra quien junte **Kmaster + R**; y cada firma exige un **nonce** fresco.

## 6. Reuso de lo existente / cambios de firmware

- `cc_kek_derive` y el AES-GCM de custodia: se reusan tal cual (R en vez de passphrase).
- Nuevos: generar/retener/entregar R; `custody_find_by_fingerprint`; nonce anti-replay por
  credencial; rama "agente" en la ceremonia (no pide pass/TOTP); rama "agente" en la firma
  (R+nonce en vez de pass+TOTP). El modo autorizacion (passphrase+TOTP, P1.3) no cambia.
