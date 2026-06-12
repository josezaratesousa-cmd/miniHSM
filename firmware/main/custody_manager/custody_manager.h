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
#define CUSTODY_CERT_MAX   2048

esp_err_t custody_init(void);
int       custody_count(void);

/* Alta: cifra priv(32B) con KEK derivada de la passphrase + chip_secret + salt aleatorio.
   Persiste priv cifrada + cert PEM + meta (alias, salt, fingerprint). *slot_out >= 0. */
esp_err_t custody_add(const char *alias, const uint8_t *priv, const char *cert_pem,
                      const uint8_t *passphrase, size_t pass_len, int *slot_out);

/* Firma digest(32B) con la credencial del slot: descifra priv con la passphrase, firma
   (ECDSA P-256 DER), zeroiza. ESP_ERR_INVALID_STATE si la passphrase es incorrecta. */
esp_err_t custody_sign(int slot, const uint8_t *passphrase, size_t pass_len,
                       const uint8_t *digest, uint8_t *sig_der_out, size_t *sig_der_len);

esp_err_t custody_get_cert(int slot, char *pem_out, size_t pem_cap);
esp_err_t custody_get_alias(int slot, char *alias_out, size_t cap);
esp_err_t custody_delete(int slot);
