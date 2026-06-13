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
#define CRYPTO_PK_SIG_MAX        512  /* firma con clave custodiada: RSA-4096=512, RSA-2048=256, ECDSA<=72 */

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

/* Igual que crypto_sign pero devuelve la firma RAW r||s (64B), para COSE/ES256. */
esp_err_t crypto_sign_raw(
    const uint8_t *digest,
    const uint8_t *privkey,
    uint8_t       *raw_out     /* CRYPTO_SIG_RAW_SIZE = 64 */
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
/* Firma un digest SHA-256 con una clave privada en PKCS#8 DER (RSA o EC).
   RSA -> PKCS#1 v1.5 ; EC -> ECDSA DER. Usado por la custodia para soportar ambos tipos. */
esp_err_t crypto_pk_sign(const uint8_t *pk8_der, size_t der_len, const uint8_t *digest,
                         uint8_t *sig_out, size_t sig_cap, size_t *sig_len_out);

/* Tipo de clave de un PKCS#8 DER (sin firmar). kind: 0=EC, 1=RSA, 2=desconocido. bits = tamano. */
typedef enum { CRYPTO_KEY_EC = 0, CRYPTO_KEY_RSA = 1, CRYPTO_KEY_UNKNOWN = 2 } crypto_key_kind_t;
esp_err_t crypto_pk_type(const uint8_t *pk8_der, size_t der_len, int *kind_out, int *bits_out);
/* Nombre legible del tipo de firma a partir de kind+bits: "EC P-256", "RSA-2048", ... */
void      crypto_sigtype_name(int kind, int bits, char *out, size_t cap);

void      crypto_zeroize(void *buf, size_t len);
