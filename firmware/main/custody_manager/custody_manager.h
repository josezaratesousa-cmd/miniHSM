#pragma once
/* Fase 1 — almacen multi-credencial (chip de custodia).
   N credenciales custodiadas, cada una con su priv cifrada en reposo
   (KEK = HKDF(passphrase||chip_secret||salt), AES-256-GCM). La clave de
   iniciacion del device (vault) queda intacta y separada. */
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "crypto_engine.h"

#define CUSTODY_MAX_CREDS  16
#define CUSTODY_ALIAS_MAX  32
#define CUSTODY_CERT_MAX   4000  /* PEM. Cert de produccion c/extensiones ~2.7KB; tope NVS string ~4000B */
#define CUSTODY_TOTP_SEED_LEN 20   /* semilla TOTP (RFC6238) */
#define CUSTODY_PRIV_DER_MAX  2048 /* PKCS#8 DER: RSA-2048~1218B, RSA-3072~1.8KB, EC P-256~138B */

esp_err_t custody_init(void);
int       custody_count(void);

/* Alta: cifra priv(32B) con KEK derivada de la passphrase + chip_secret + salt aleatorio.
   Persiste priv cifrada + cert PEM + meta (alias, salt, fingerprint). *slot_out >= 0. */
/* priv_der = clave privada en PKCS#8 DER (RSA o EC P-256). Se cifra con la KEK y se guarda
   (longitud variable). El cert va en claro. */
esp_err_t custody_add(const char *alias, const uint8_t *priv_der, size_t priv_der_len,
                      const char *cert_pem, const uint8_t *passphrase, size_t pass_len,
                      uint8_t *totp_seed_out, size_t *totp_seed_len_out, int *slot_out, int mode);

/* Firma digest(32B) con la credencial del slot: descifra el PKCS#8 con la passphrase y firma
   con mbedtls_pk (RSA -> PKCS#1 v1.5 ; EC -> ECDSA DER). sig_der_out debe tener CRYPTO_PK_SIG_MAX.
   ESP_ERR_INVALID_STATE si la passphrase/TOTP es incorrecta. */
esp_err_t custody_sign(int slot, const uint8_t *passphrase, size_t pass_len,
                       const char *totp_code, uint64_t unix_time,
                       const uint8_t *digest, uint8_t *sig_der_out, size_t *sig_der_len);

/* Tipo de firma de la credencial del slot (sin passphrase). kind: 0=EC,1=RSA. bits=tamano.
   Para listar credenciales y reportar el algoritmo al server (multi-cert). */
esp_err_t custody_get_type(int slot, int *kind_out, int *bits_out);
/* Modo de la credencial: 0=agente (sin TOTP), 1=autorizacion (TOTP por firma). */
esp_err_t custody_get_mode(int slot, int *mode_out);

esp_err_t custody_get_cert(int slot, char *pem_out, size_t pem_cap);
esp_err_t custody_get_alias(int slot, char *alias_out, size_t cap);
/* Fingerprint (SHA-256 de la pubkey) en hex de la credencial del slot, para listar en la VC. */
esp_err_t custody_get_fingerprint(int slot, char *hex_out, size_t cap);
esp_err_t custody_delete(int slot);
