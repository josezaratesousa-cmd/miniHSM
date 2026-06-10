#include "cert_manager.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "crypto_engine.h"
#include "vault_manager.h"

/*
 * mbedTLS 4.x (ESP-IDF 6.x) reorganizo los headers bajo TF-PSA-Crypto.
 * Los headers de X.509 (parse/write) siguen accesibles via mbedtls/x509_crt.h
 * pero los de ECC, entropy y ctr_drbg fueron absorbidos por PSA.
 * Este archivo usa PSA para crypto y mbedtls solo para X.509 write/parse.
 */
#include "mbedtls/x509_crt.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/x509write_crt.h"
#include "mbedtls/x509write_csr.h"
#include "mbedtls/pk.h"
#include "psa/crypto.h"

static const char *TAG       = "cert_manager";
static const char *NVS_NS    = "cert";
static const char *NVS_PEM   = "cert_pem";
static const char *NVS_STATE = "state";

static char         s_cert_pem[CERT_PEM_MAX_SIZE];
static cert_state_t s_state       = CERT_STATE_UNPROVISIONED;
static int          s_initialized = 0;

/* ─────────────────────────────────────────────────────────────────────────── */
/*  NVS                                                                         */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t nvs_save_cert(const char *pem, cert_state_t state)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, NVS_PEM, pem);
    if (err == ESP_OK) err = nvs_set_u8(h, NVS_STATE, (uint8_t)state);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t nvs_load_cert(char *pem_out, cert_state_t *state_out)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t len = CERT_PEM_MAX_SIZE;
    err = nvs_get_str(h, NVS_PEM, pem_out, &len);
    if (err == ESP_OK) {
        uint8_t s = 0;
        nvs_get_u8(h, NVS_STATE, &s);
        *state_out = (cert_state_t)s;
    }
    nvs_close(h);
    return err;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  RNG wrapper para mbedtls_x509write usando PSA                              */
/* ─────────────────────────────────────────────────────────────────────────── */

static int psa_rng_for_mbedtls(void *ctx, unsigned char *buf, size_t len)
{
    (void)ctx;
    psa_status_t s = psa_generate_random(buf, len);
    return (s == PSA_SUCCESS) ? 0 : -1;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Carga keypair en mbedtls_pk_context usando PSA export                      */
/* ─────────────────────────────────────────────────────────────────────────── */

static int load_pk_from_vault(mbedtls_pk_context *pk)
{
    uint8_t privkey[CRYPTO_PRIVKEY_SIZE];
    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];

    if (vault_get_privkey_raw(privkey, pubkey) != ESP_OK) return -1;

    /*
     * mbedtls_pk con ECKEY en mbedTLS 4.x: los campos internos del
     * ecp_keypair se acceden via PSA internamente. Usamos la API publica
     * mbedtls_pk_parse_key para cargar la clave privada desde DER/raw.
     *
     * Alternativa mas robusta: construir un PEM temporal de la clave privada
     * en formato SEC1 y parsearlo con mbedtls_pk_parse_key.
     */

    /* Construir clave privada en formato SEC1 DER para mbedtls_pk_parse_key.
       SEC1 P-256 privkey DER = header (7 bytes) + privkey (32) + pubkey tag + pubkey (65) */
    unsigned char sec1_der[120];
    size_t sec1_len = 0;

    /* Header SEC1 para P-256:
       30 77          SEQUENCE
         02 01 01     version = 1
         04 20        OCTET STRING, 32 bytes (privkey)
           <32 bytes>
         a1 5b        [1] EXPLICIT (optional pubkey)
           03 59      BIT STRING
             00       no unused bits
             04       uncompressed point prefix
             <64 bytes X||Y>
    */
    const unsigned char hdr[] = {
        0x30, 0x77,
        0x02, 0x01, 0x01,
        0x04, 0x20
    };
    memcpy(sec1_der, hdr, sizeof(hdr));
    sec1_len = sizeof(hdr);
    memcpy(sec1_der + sec1_len, privkey, 32);
    sec1_len += 32;

    const unsigned char pub_hdr[] = {
        0xa1, 0x5b,
        0x03, 0x59,
        0x00
    };
    memcpy(sec1_der + sec1_len, pub_hdr, sizeof(pub_hdr));
    sec1_len += sizeof(pub_hdr);
    memcpy(sec1_der + sec1_len, pubkey, 65);  /* 0x04 || X || Y */
    sec1_len += 65;

    /* Total: 7 + 32 + 5 + 65 = 109 bytes. sec1_der[1] = total - 2 = 107 = 0x6b */
    sec1_der[1] = (unsigned char)(sec1_len - 2);

    mbedtls_pk_init(pk);
    int ret = mbedtls_pk_parse_key(pk, sec1_der, sec1_len, NULL, 0,
                                    psa_rng_for_mbedtls, NULL);

    crypto_zeroize(privkey, sizeof(privkey));
    crypto_zeroize(sec1_der, sizeof(sec1_der));

    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_pk_parse_key failed: -0x%04X", (unsigned)-ret);
        mbedtls_pk_free(pk);
    }
    return ret;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Genera certificado autofirmado                                              */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t generate_selfsigned(void)
{
    ESP_LOGI(TAG, "Generating self-signed certificate...");

    mbedtls_pk_context     pk;
    mbedtls_x509write_cert crt;

    mbedtls_x509write_crt_init(&crt);
    esp_err_t result = ESP_FAIL;

    if (load_pk_from_vault(&pk) != 0) {
        ESP_LOGE(TAG, "Failed to load keypair");
        goto cleanup;
    }

    char device_id[17];
    vault_get_device_id(device_id);
    char subject[CERT_SUBJECT_MAX];
    snprintf(subject, sizeof(subject), "CN=MiniHSM-%s,O=MiniHSM,C=PE", device_id);

    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &pk);
    mbedtls_x509write_crt_set_issuer_key(&crt, &pk);
    mbedtls_x509write_crt_set_subject_name(&crt, subject);
    mbedtls_x509write_crt_set_issuer_name(&crt, subject);
    mbedtls_x509write_crt_set_serial_new(&crt);
    mbedtls_x509write_crt_set_validity(&crt, "20240101000000", "20340101000000");
    mbedtls_x509write_crt_set_key_usage(&crt, MBEDTLS_X509_KU_DIGITAL_SIGNATURE);
    mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);

    unsigned char pem_buf[CERT_PEM_MAX_SIZE];
    int ret = mbedtls_x509write_crt_pem(&crt, pem_buf, sizeof(pem_buf),
                                         psa_rng_for_mbedtls, NULL);
    if (ret != 0) {
        ESP_LOGE(TAG, "x509write_crt_pem failed: -0x%04X", (unsigned)-ret);
        goto cleanup;
    }

    strncpy(s_cert_pem, (char *)pem_buf, CERT_PEM_MAX_SIZE - 1);
    s_state = CERT_STATE_UNPROVISIONED;
    nvs_save_cert(s_cert_pem, s_state);
    ESP_LOGI(TAG, "Self-signed certificate generated OK");
    result = ESP_OK;

cleanup:
    mbedtls_pk_free(&pk);
    mbedtls_x509write_crt_free(&crt);
    return result;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  API publica                                                                 */
/* ─────────────────────────────────────────────────────────────────────────── */

esp_err_t cert_manager_init(void)
{
    esp_err_t err = nvs_load_cert(s_cert_pem, &s_state);

    if (err == ESP_ERR_NVS_NOT_FOUND || strlen(s_cert_pem) == 0) {
        ESP_LOGI(TAG, "No cert found, generating self-signed...");
        err = generate_selfsigned();
    } else if (err == ESP_OK) {
        ESP_LOGI(TAG, "Cert loaded (state=%s)",
            s_state == CERT_STATE_PROVISIONED ? "PROVISIONED" : "UNPROVISIONED");
    }

    if (err == ESP_OK) s_initialized = 1;
    return err;
}

esp_err_t cert_get_pem(char *pem_out, size_t pem_buf_size)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    size_t len = strlen(s_cert_pem);
    if (len == 0 || len >= pem_buf_size) return ESP_FAIL;
    memcpy(pem_out, s_cert_pem, len + 1);
    return ESP_OK;
}

esp_err_t cert_get_csr(char *csr_out, size_t csr_buf_size)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    mbedtls_pk_context    pk;
    mbedtls_x509write_csr csr;

    mbedtls_x509write_csr_init(&csr);
    esp_err_t result = ESP_FAIL;

    if (load_pk_from_vault(&pk) != 0) goto cleanup;

    char device_id[17];
    vault_get_device_id(device_id);
    char subject[CERT_SUBJECT_MAX];
    snprintf(subject, sizeof(subject), "CN=MiniHSM-%s,O=MiniHSM,C=PE", device_id);

    mbedtls_x509write_csr_set_key(&csr, &pk);
    mbedtls_x509write_csr_set_subject_name(&csr, subject);
    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);
    mbedtls_x509write_csr_set_key_usage(&csr, MBEDTLS_X509_KU_DIGITAL_SIGNATURE);

    int ret = mbedtls_x509write_csr_pem(&csr, (unsigned char *)csr_out,
                                         csr_buf_size,
                                         psa_rng_for_mbedtls, NULL);
    if (ret == 0) {
        result = ESP_OK;
        ESP_LOGI(TAG, "CSR generated OK");
    } else {
        ESP_LOGE(TAG, "CSR pem failed: -0x%04X", (unsigned)-ret);
    }

cleanup:
    mbedtls_pk_free(&pk);
    mbedtls_x509write_csr_free(&csr);
    return result;
}

esp_err_t cert_load_ca_signed(const char *cert_pem, size_t cert_len)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    mbedtls_x509_crt new_cert;
    mbedtls_x509_crt_init(&new_cert);

    int ret = mbedtls_x509_crt_parse(&new_cert,
                                      (const unsigned char *)cert_pem,
                                      cert_len + 1);
    mbedtls_x509_crt_free(&new_cert);

    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse cert: -0x%04X", (unsigned)-ret);
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_cert_pem, cert_pem, CERT_PEM_MAX_SIZE - 1);
    s_cert_pem[CERT_PEM_MAX_SIZE - 1] = '\0';
    s_state = CERT_STATE_PROVISIONED;
    nvs_save_cert(s_cert_pem, s_state);
    ESP_LOGI(TAG, "CA cert loaded — PROVISIONED");
    return ESP_OK;
}

cert_state_t cert_get_state(void)
{
    return s_state;
}

esp_err_t cert_get_fingerprint(char *fingerprint_out)
{
    if (!s_initialized || strlen(s_cert_pem) == 0) return ESP_ERR_INVALID_STATE;
    uint8_t hash[CRYPTO_DIGEST_SIZE];
    crypto_sha256((const uint8_t *)s_cert_pem, strlen(s_cert_pem), hash);
    crypto_bytes_to_hex(hash, CRYPTO_DIGEST_SIZE, fingerprint_out);
    return ESP_OK;
}
