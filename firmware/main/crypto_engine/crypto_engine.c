#include "crypto_engine.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "psa/crypto.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "esp_random.h"

static const char *TAG = "crypto_engine";
static int s_initialized = 0;

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Init                                                                        */
/* ─────────────────────────────────────────────────────────────────────────── */

esp_err_t crypto_engine_init(void)
{
    if (s_initialized) return ESP_OK;

    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init failed: %d", (int)status);
        return ESP_FAIL;
    }

    s_initialized = 1;
    ESP_LOGI(TAG, "Crypto engine OK (PSA / P-256 / DER output)");
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  raw r||s  →  DER  SEQUENCE { INTEGER r, INTEGER s }                        */
/* ─────────────────────────────────────────────────────────────────────────── */

esp_err_t crypto_sig_raw_to_der(const uint8_t *raw, uint8_t *der_out, size_t *der_len_out)
{
    if (!raw || !der_out || !der_len_out) return ESP_ERR_INVALID_ARG;

    const uint8_t *r = raw;
    const uint8_t *s = raw + 32;

    /* Si el byte mas alto tiene el bit 7 a 1 hay que poner un 0x00 de padding
       para que el INTEGER DER no sea negativo */
    int r_pad = (r[0] & 0x80) ? 1 : 0;
    int s_pad = (s[0] & 0x80) ? 1 : 0;

    size_t r_len    = 32 + r_pad;
    size_t s_len    = 32 + s_pad;
    size_t seq_body = 2 + r_len + 2 + s_len;   /* INTEGER r + INTEGER s */
    size_t total    = 2 + seq_body;              /* SEQUENCE header */

    if (total > CRYPTO_SIG_DER_MAX_SIZE) return ESP_ERR_INVALID_SIZE;

    uint8_t *p = der_out;
    *p++ = 0x30;                   /* SEQUENCE  */
    *p++ = (uint8_t)seq_body;
    *p++ = 0x02;                   /* INTEGER r */
    *p++ = (uint8_t)r_len;
    if (r_pad) *p++ = 0x00;
    memcpy(p, r, 32); p += 32;
    *p++ = 0x02;                   /* INTEGER s */
    *p++ = (uint8_t)s_len;
    if (s_pad) *p++ = 0x00;
    memcpy(p, s, 32);

    *der_len_out = total;
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Keypair generation                                                          */
/* ─────────────────────────────────────────────────────────────────────────── */

esp_err_t crypto_generate_keypair(uint8_t *privkey_out, uint8_t *pubkey_out)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attrs, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attrs, 256);
    psa_set_key_usage_flags(&attrs,
        PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_EXPORT);
    psa_set_key_algorithm(&attrs, PSA_ALG_ECDSA(PSA_ALG_SHA_256));

    psa_key_id_t key_id;
    psa_status_t status = psa_generate_key(&attrs, &key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_generate_key failed: %d", (int)status);
        return ESP_FAIL;
    }

    size_t len = 0;
    status = psa_export_key(key_id, privkey_out, CRYPTO_PRIVKEY_SIZE, &len);
    if (status != PSA_SUCCESS) { psa_destroy_key(key_id); return ESP_FAIL; }

    status = psa_export_public_key(key_id, pubkey_out, CRYPTO_PUBKEY_SIZE, &len);
    psa_destroy_key(key_id);
    if (status != PSA_SUCCESS) return ESP_FAIL;

    ESP_LOGI(TAG, "Keypair generated OK");
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Sign — devuelve firma en DER (PAdES / CAdES / XAdES ready)                 */
/* ─────────────────────────────────────────────────────────────────────────── */

esp_err_t crypto_sign(
    const uint8_t *digest,
    const uint8_t *privkey,
    uint8_t       *sig_der_out,
    size_t        *sig_der_len_out)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attrs, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attrs, 256);
    psa_set_key_usage_flags(&attrs, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_algorithm(&attrs, PSA_ALG_ECDSA(PSA_ALG_SHA_256));

    psa_key_id_t key_id;
    psa_status_t status = psa_import_key(&attrs, privkey, CRYPTO_PRIVKEY_SIZE, &key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key failed: %d", (int)status);
        return ESP_FAIL;
    }

    /* PSA produce raw r||s (64 bytes) */
    uint8_t raw_sig[CRYPTO_SIG_RAW_SIZE];
    size_t  raw_len = 0;
    status = psa_sign_hash(
        key_id,
        PSA_ALG_ECDSA(PSA_ALG_SHA_256),
        digest, CRYPTO_DIGEST_SIZE,
        raw_sig, CRYPTO_SIG_RAW_SIZE,
        &raw_len
    );
    psa_destroy_key(key_id);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_sign_hash failed: %d", (int)status);
        return ESP_FAIL;
    }

    /* Convertir a DER para PAdES/CAdES/XAdES */
    esp_err_t err = crypto_sig_raw_to_der(raw_sig, sig_der_out, sig_der_len_out);
    crypto_zeroize(raw_sig, sizeof(raw_sig));

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "raw_to_der conversion failed");
        return err;
    }

    ESP_LOGD(TAG, "Sign OK: DER len=%d", (int)*sig_der_len_out);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Verify — acepta firma DER                                                   */
/* ─────────────────────────────────────────────────────────────────────────── */

esp_err_t crypto_verify(
    const uint8_t *digest,
    const uint8_t *sig_der,
    size_t         sig_der_len,
    const uint8_t *pubkey,
    int           *valid_out)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    *valid_out = 0;

    /* Decodificar DER → raw r||s */
    if (sig_der_len < 8 || sig_der[0] != 0x30) return ESP_ERR_INVALID_ARG;

    uint8_t raw[CRYPTO_SIG_RAW_SIZE];
    memset(raw, 0, sizeof(raw));

    /* Parseo manual del DER SEQUENCE { INTEGER r, INTEGER s } */
    const uint8_t *p   = sig_der + 2; /* saltar 0x30 len */
    if (*p++ != 0x02) return ESP_ERR_INVALID_ARG;
    size_t r_len = *p++;
    const uint8_t *r   = p;
    p += r_len;
    if (*p++ != 0x02) return ESP_ERR_INVALID_ARG;
    size_t s_len = *p++;
    const uint8_t *s   = p;

    /* Copiar los 32 bytes efectivos (saltando padding 0x00 si hay) */
    if (r_len > 32) { memcpy(raw,      r + (r_len - 32), 32); }
    else            { memcpy(raw + (32 - r_len), r, r_len);   }
    if (s_len > 32) { memcpy(raw + 32, s + (s_len - 32), 32); }
    else            { memcpy(raw + 32 + (32 - s_len), s, s_len); }

    psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attrs, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attrs, 256);
    psa_set_key_usage_flags(&attrs, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&attrs, PSA_ALG_ECDSA(PSA_ALG_SHA_256));

    psa_key_id_t key_id;
    psa_status_t status = psa_import_key(&attrs, pubkey, CRYPTO_PUBKEY_SIZE, &key_id);
    if (status != PSA_SUCCESS) return ESP_FAIL;

    status = psa_verify_hash(
        key_id,
        PSA_ALG_ECDSA(PSA_ALG_SHA_256),
        digest, CRYPTO_DIGEST_SIZE,
        raw, CRYPTO_SIG_RAW_SIZE
    );
    psa_destroy_key(key_id);
    *valid_out = (status == PSA_SUCCESS) ? 1 : 0;
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  SHA-256                                                                     */
/* ─────────────────────────────────────────────────────────────────────────── */

esp_err_t crypto_sha256(const uint8_t *data, size_t len, uint8_t *hash_out)
{
    size_t hash_len = 0;
    psa_status_t status = psa_hash_compute(
        PSA_ALG_SHA_256, data, len,
        hash_out, CRYPTO_DIGEST_SIZE, &hash_len
    );
    return (status == PSA_SUCCESS) ? ESP_OK : ESP_FAIL;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Helpers                                                                     */
/* ─────────────────────────────────────────────────────────────────────────── */

void crypto_bytes_to_hex(const uint8_t *bytes, size_t len, char *hex_out)
{
    for (size_t i = 0; i < len; i++) sprintf(hex_out + i * 2, "%02x", bytes[i]);
    hex_out[len * 2] = '\0';
}

esp_err_t crypto_hex_to_bytes(const char *hex, uint8_t *bytes_out, size_t expected_len)
{
    if (strlen(hex) != expected_len * 2) return ESP_ERR_INVALID_ARG;
    for (size_t i = 0; i < expected_len; i++) {
        unsigned int b;
        if (sscanf(hex + i * 2, "%02x", &b) != 1) return ESP_ERR_INVALID_ARG;
        bytes_out[i] = (uint8_t)b;
    }
    return ESP_OK;
}

void crypto_zeroize(void *buf, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)buf;
    while (len--) *p++ = 0;
}

/* RNG para mbedtls (HW RNG del ESP32-S3; buena entropia con radio activa). */
static int crypto_rng_cb(void *ctx, unsigned char *buf, size_t len) {
    (void)ctx; esp_fill_random(buf, len); return 0;
}

/* Firma un digest SHA-256 con una clave en PKCS#8 DER (RSA o EC). mbedtls_pk detecta el tipo:
   RSA -> PKCS#1 v1.5 ; EC -> ECDSA DER. */
esp_err_t crypto_pk_sign(const uint8_t *pk8_der, size_t der_len, const uint8_t *digest,
                         uint8_t *sig_out, size_t sig_cap, size_t *sig_len_out) {
    if (!pk8_der || !digest || !sig_out || !sig_len_out) return ESP_ERR_INVALID_ARG;
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    int ret = mbedtls_pk_parse_key(&pk, pk8_der, der_len, NULL, 0, crypto_rng_cb, NULL);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_pk_parse_key: -0x%04x", (unsigned)(-ret));
        mbedtls_pk_free(&pk);
        return ESP_FAIL;
    }
    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, digest, CRYPTO_DIGEST_SIZE,
                          sig_out, sig_cap, sig_len_out, crypto_rng_cb, NULL);
    mbedtls_pk_free(&pk);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_pk_sign: -0x%04x", (unsigned)(-ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* Detecta el tipo de clave de un PKCS#8 DER sin firmar (para listar/reportar el algoritmo). */
esp_err_t crypto_pk_type(const uint8_t *pk8_der, size_t der_len, int *kind_out, int *bits_out) {
    if (!pk8_der || !kind_out) return ESP_ERR_INVALID_ARG;
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    int ret = mbedtls_pk_parse_key(&pk, pk8_der, der_len, NULL, 0, crypto_rng_cb, NULL);
    if (ret != 0) { mbedtls_pk_free(&pk); return ESP_FAIL; }
    mbedtls_pk_type_t t = mbedtls_pk_get_type(&pk);
    if (t == MBEDTLS_PK_RSA)                              *kind_out = CRYPTO_KEY_RSA;
    else if (t == MBEDTLS_PK_ECKEY || t == MBEDTLS_PK_ECDSA) *kind_out = CRYPTO_KEY_EC;
    else                                                 *kind_out = CRYPTO_KEY_UNKNOWN;
    if (bits_out) *bits_out = (int)mbedtls_pk_get_bitlen(&pk);
    mbedtls_pk_free(&pk);
    return ESP_OK;
}

/* "EC P-256" / "RSA-2048" / ... a partir de kind+bits. */
void crypto_sigtype_name(int kind, int bits, char *out, size_t cap) {
    if (kind == CRYPTO_KEY_RSA) {
        snprintf(out, cap, "RSA-%d", bits);
    } else if (kind == CRYPTO_KEY_EC) {
        const char *c = (bits==256)?"P-256":(bits==384)?"P-384":(bits==521)?"P-521":"EC";
        snprintf(out, cap, "EC %s", c);
    } else {
        snprintf(out, cap, "unknown");
    }
}

/* Firma raw r||s (64B) para COSE/ES256 (sin conversion a DER). */
esp_err_t crypto_sign_raw(const uint8_t *digest, const uint8_t *privkey, uint8_t *raw_out) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attrs, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attrs, 256);
    psa_set_key_usage_flags(&attrs, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_algorithm(&attrs, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    psa_key_id_t key_id;
    psa_status_t status = psa_import_key(&attrs, privkey, CRYPTO_PRIVKEY_SIZE, &key_id);
    if (status != PSA_SUCCESS) { ESP_LOGE(TAG, "psa_import_key(raw) failed: %d", (int)status); return ESP_FAIL; }
    size_t raw_len = 0;
    status = psa_sign_hash(key_id, PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                           digest, CRYPTO_DIGEST_SIZE, raw_out, CRYPTO_SIG_RAW_SIZE, &raw_len);
    psa_destroy_key(key_id);
    if (status != PSA_SUCCESS || raw_len != CRYPTO_SIG_RAW_SIZE) {
        ESP_LOGE(TAG, "psa_sign_hash(raw) failed: %d len=%d", (int)status, (int)raw_len);
        return ESP_FAIL;
    }
    return ESP_OK;
}
