#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/* P-256: clave privada 32 bytes, publica uncompressed 65 bytes (04|X|Y) */
#define CRYPTO_PRIVKEY_SIZE      32
#define CRYPTO_PUBKEY_SIZE       65
#define CRYPTO_DIGEST_SIZE       32   /* SHA-256 */

/* ECDSA P-256 en DER: SEQUENCE { INTEGER r, INTEGER s }
   Maximo: 2 + 2 + 33 + 2 + 33 = 72 bytes */
#define CRYPTO_SIG_RAW_SIZE      64   /* r||s raw (PSA output)  */
#define CRYPTO_SIG_DER_MAX_SIZE  72   /* DER encoded (PAdES/CAdES/XAdES) */

esp_err_t crypto_engine_init(void);

esp_err_t crypto_generate_keypair(
    uint8_t *privkey_out,
    uint8_t *pubkey_out
);

/* Firma digest y devuelve firma en DER (listo para PAdES/CAdES/XAdES) */
esp_err_t crypto_sign(
    const uint8_t *digest,
    const uint8_t *privkey,
    uint8_t       *sig_der_out,
    size_t        *sig_der_len_out
);

esp_err_t crypto_verify(
    const uint8_t *digest,
    const uint8_t *sig_der,
    size_t         sig_der_len,
    const uint8_t *pubkey,
    int           *valid_out
);

esp_err_t crypto_sha256(
    const uint8_t *data,
    size_t         len,
    uint8_t       *hash_out
);

/* Convierte firma raw r||s (64 bytes) a DER */
esp_err_t crypto_sig_raw_to_der(
    const uint8_t *raw,
    uint8_t       *der_out,
    size_t        *der_len_out
);

void      crypto_bytes_to_hex(const uint8_t *bytes, size_t len, char *hex_out);
esp_err_t crypto_hex_to_bytes(const char *hex, uint8_t *bytes_out, size_t expected_len);
void      crypto_zeroize(void *buf, size_t len);
