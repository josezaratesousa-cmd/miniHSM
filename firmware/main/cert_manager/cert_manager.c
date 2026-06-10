#include "cert_manager.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "crypto_engine.h"
#include "vault_manager.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

static const char *TAG       = "cert_manager";
static const char *NVS_NS    = "cert";
static const char *NVS_PEM   = "cert_pem";
static const char *NVS_STATE = "state";
static char         s_cert_pem[CERT_PEM_MAX_SIZE];
static cert_state_t s_state       = CERT_STATE_UNPROVISIONED;
static int          s_initialized = 0;

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

/* Carga la clave privada del vault en un mbedtls_pk_context.
 * Usa mbedtls_ecp_read_key() que carga la clave Y calcula Q (pubkey).
 * La clave raw se zeroiza antes de retornar. Caller debe mbedtls_pk_free(). */
static int load_pk_from_vault(mbedtls_pk_context *pk)
{
    uint8_t privkey[CRYPTO_PRIVKEY_SIZE];
    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];

    if (vault_get_privkey_raw(privkey, pubkey) != ESP_OK) {
        crypto_zeroize(privkey, sizeof(privkey));
        return -1;
    }

    mbedtls_pk_init(pk);
    int ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) goto done;

    /* mbedtls_ecp_read_key: carga privkey en el keypair y calcula Q.
     * mbedtls_pk_ec() esta deprecated en mbedTLS 3.4+ pero funciona
     * (el flag -Wno-error=deprecated-declarations lo permite). */
    ret = mbedtls_ecp_read_key(MBEDTLS_ECP_DP_SECP256R1,
                                mbedtls_pk_ec(*pk),
                                privkey, CRYPTO_PRIVKEY_SIZE);
done:
    crypto_zeroize(privkey, sizeof(privkey));
    crypto_zeroize(pubkey,  sizeof(pubkey));
    if (ret != 0) mbedtls_pk_free(pk);
    return ret;
}

static esp_err_t generate_selfsigned(void)
{
    ESP_LOGI(TAG, "Generating self-signed certificate...");
    mbedtls_pk_context       pk;
    mbedtls_x509write_cert   crt;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_x509write_crt_init(&crt);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const unsigned char *)"minihsm_cert", 12);

    if (load_pk_from_vault(&pk) != 0) {
        ESP_LOGE(TAG, "Failed to load keypair");
        goto cleanup;
    }

    char device_id[17];
    vault_get_device_id(device_id);
    char subject[CERT_SUBJECT_MAX];
    snprintf(subject, sizeof(subject), "CN=MiniHSM-%s,O=MiniHSM,C=PE", device_id);

    /* Serial = primeros 8 bytes del hash de la pubkey (siempre positivo) */
    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];
    vault_get_pubkey(pubkey);
    uint8_t hash[CRYPTO_DIGEST_SIZE];
    crypto_sha256(pubkey, CRYPTO_PUBKEY_SIZE, hash);
    hash[0] &= 0x7F; /* bit mas significativo = 0 para DER INTEGER positivo */

    mbedtls_x509write_crt_set_version(&crt,  MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt,   MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt,  &pk);
    mbedtls_x509write_crt_set_issuer_key(&crt,   &pk);
    mbedtls_x509write_crt_set_subject_name(&crt, subject);
    mbedtls_x509write_crt_set_issuer_name(&crt,  subject);
    mbedtls_x509write_crt_set_serial_raw(&crt, hash, 8);
    mbedtls_x509write_crt_set_validity(&crt, "20240101000000", "20340101000000");
    mbedtls_x509write_crt_set_key_usage(&crt, MBEDTLS_X509_KU_DIGITAL_SIGNATURE);
    mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);

    unsigned char pem_buf[CERT_PEM_MAX_SIZE];
    int ret = mbedtls_x509write_crt_pem(&crt, pem_buf, sizeof(pem_buf),
                                         mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        ESP_LOGE(TAG, "x509write_crt_pem failed: %d", ret);
        goto cleanup;
    }
    strncpy(s_cert_pem, (char *)pem_buf, CERT_PEM_MAX_SIZE - 1);
    s_state = CERT_STATE_UNPROVISIONED;
    nvs_save_cert(s_cert_pem, s_state);
    ESP_LOGI(TAG, "Self-signed cert OK");

cleanup:
    mbedtls_pk_free(&pk);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return (strlen(s_cert_pem) > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t cert_manager_init(void)
{
    esp_err_t err = nvs_load_cert(s_cert_pem, &s_state);
    if (err == ESP_ERR_NVS_NOT_FOUND || strlen(s_cert_pem) == 0) {
        ESP_LOGI(TAG, "No cert — generating self-signed...");
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
    ESP_LOGI(TAG, "Generating CSR for CA ceremony...");
    mbedtls_pk_context       pk;
    mbedtls_x509write_csr    csr;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509write_csr_init(&csr);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const unsigned char *)"minihsm_csr", 11);
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
                  csr_buf_size, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret == 0) {
        result = ESP_OK;
        ESP_LOGI(TAG, "CSR OK — send to CA for signing");
    } else {
        ESP_LOGE(TAG, "CSR failed: %d", ret);
    }
cleanup:
    mbedtls_pk_free(&pk);
    mbedtls_x509write_csr_free(&csr);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return result;
}

esp_err_t cert_load_ca_signed(const char *cert_pem, size_t cert_len)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    mbedtls_x509_crt c;
    mbedtls_x509_crt_init(&c);
    int ret = mbedtls_x509_crt_parse(&c,
                  (const unsigned char *)cert_pem, cert_len + 1);
    mbedtls_x509_crt_free(&c);
    if (ret != 0) {
        ESP_LOGE(TAG, "Invalid cert PEM: -0x%04X", (unsigned)-ret);
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(s_cert_pem, cert_pem, CERT_PEM_MAX_SIZE - 1);
    s_cert_pem[CERT_PEM_MAX_SIZE - 1] = '\0';
    s_state = CERT_STATE_PROVISIONED;
    nvs_save_cert(s_cert_pem, s_state);
    ESP_LOGI(TAG, "CA-signed cert loaded — device is PROVISIONED");
    return ESP_OK;
}

cert_state_t cert_get_state(void) { return s_state; }

esp_err_t cert_get_fingerprint(char *fingerprint_out)
{
    if (!s_initialized || strlen(s_cert_pem) == 0)
        return ESP_ERR_INVALID_STATE;
    uint8_t hash[CRYPTO_DIGEST_SIZE];
    crypto_sha256((const uint8_t *)s_cert_pem, strlen(s_cert_pem), hash);
    crypto_bytes_to_hex(hash, CRYPTO_DIGEST_SIZE, fingerprint_out);
    return ESP_OK;
}
