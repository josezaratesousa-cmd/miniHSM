#pragma once
/* Agente automatizado — self-test del esquema de cifrado de la credencial.
 * Valida en el hardware real la cadena:
 *   K = cc_kek_derive(R, chip_kek_secret, salt=fingerprint)  (HKDF-SHA256)
 *   AES-256-GCM(K) cifra/descifra la clave privada  (round-trip + tag).
 * No toca el vault ni las credenciales; no bloquea el boot. */
void agent_selftest(void);
