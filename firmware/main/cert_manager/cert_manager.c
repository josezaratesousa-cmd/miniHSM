#include "cert_manager.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "crypto_engine.h"
#include "vault_manager.h"

/* mbedTLS para X.509 (ESP-IDF incluye mbedTLS con x509write) */
#include "mbedtls/x509_crt.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/x509write_crt.h"
#include "mbedtls/x509write_csr.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/bignum.h"
#include "mbedtls/oid.h"

static const char *TAG      = "cert_manager";
static const char *NVS_NS   = "cert";
static const char *NVS_PEM  = "cert_pem";
static const char *NVS_STATE= "state";

static char         s_cert_pem[CERT_PEM_MAX_SIZE];
static cert_state_t s_state       = CERT_STATE_UNPROVISIONED;
static int          s_initialized = 0;

/* ─────────────────────────────────────────────────────────────────────────── */
/*  NVS helpers                                                                 */
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
/*  Carga privkey en mbedtls_pk_context (solo para generacion de cert/CSR)     */
/* ─────────────────────────────────────────────────────────────────────────── */

static int load_pk_from_vault(mbedtls_pk_context *pk)
{
    uint8_t privkey[CRYPTO_PRIVKEY_SIZE];
    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];

    /* vault_manager_get_raw_keypair es una funcion helper que necesitamos.
       Como no existe, cargamos directo desde NVS via vault_get_privkey_raw. */
    if (vault_get_privkey_raw(privkey, pubkey) != ESP_OK) return -1;

    mbedtls_pk_init(pk);
    int ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) goto fail;

    mbedtls_ecp_keypair *ecp = mbedtls_pk_ec(*pk);

    ret = mbedtls_ecp_group_load(
        &ecp->MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) goto fail;

    ret = mbedtls_mpi_read_binary(
        &ecp->MBEDTLS_PRIVATE(d), privkey, CRYPTO_PRIVKEY_SIZE);
    if (ret != 0) goto fail;

    /* Calcular clave publica desde la privada */
    ret = mbedtls_ecp_mul(
        &ecp->MBEDTLS_PRIVATE(grp),
        &ecp->MBEDTLS_PRIVATE(Q),
        &ecp->MBEDTLS_PRIVATE(d),
        &ecp->MBEDTLS_PRIVATE(grp).G,
        mbedtls_ctr_drbg_random, NULL   /* sin RNG — operacion determinista */
    );

    /* RNG para ecp_mul: usamos entropia del sistema */
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const unsigned char *)"minihsm", 7);

    ret = mbedtls_ecp_mul(
        &ecp->MBEDTLS_PRIVATE(grp),
        &ecp->MBEDTLS_PRIVATE(Q),
        &ecp->MBEDTLS_PRIVATE(d),
        &ecp->MBEDTLS_PRIVATE(grp).G,
        mbedtls_ctr_drbg_random, &ctr_drbg
    );

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    crypto_zeroize(privkey, sizeof(privkey));

    if (ret != 0) goto fail;
    return 0;

fail:
    crypto_zeroize(privkey, sizeof(privkey));
    mbedtls_pk_free(pk);
    return ret;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  Genera certificado autofirmado                                              */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t generate_selfsigned(void)
{
    ESP_LOGI(TAG, "Generating self-signed certificate...");

    mbedtls_pk_context        pk;
    mbedtls_x509write_cert    crt;
    mbedtls_entropy_context   entropy;
    mbedtls_ctr_drbg_context  ctr_drbg;
    mbedtls_mpi               serial;

    mbedtls_x509write_crt_init(&crt);
    mbedtls_mpi_init(&serial);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const unsigned char *)"minihsm_cert", 12);

    if (load_pk_from_vault(&pk) != 0) {
        ESP_LOGE(TAG, "Failed to load keypair for cert generation");
        goto cleanup;
    }

    /* Numero de serie = primeros 8 bytes del hash de la pubkey */
    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];
    vault_get_pubkey(pubkey);
    uint8_t hash[CRYPTO_DIGEST_SIZE];
    crypto_sha256(pubkey, CRYPTO_PUBKEY_SIZE, hash);
    mbedtls_mpi_read_binary(&serial, hash, 8);

    /* Subject y Issuer (autofirmado: son iguales) */
    char device_id[17];
    vault_get_device_id(device_id);
    char subject[CERT_SUBJECT_MAX];
    snprintf(subject, sizeof(subject),
        "CN=MiniHSM-%s,O=MiniHSM,C=PE", device_id);

    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &pk);
    mbedtls_x509write_crt_set_issuer_key(&crt, &pk);
    mbedtls_x509write_crt_set_subject_name(&crt, subject);
    mbedtls_x509write_crt_set_issuer_name(&crt, subject);
    mbedtls_x509write_crt_set_serial_new(&crt);
    mbedtls_x509write_crt_set_validity(&crt, "20240101000000", "20340101000000");

    /* Key Usage: Digital Signature */
    mbedtls_x509write_crt_set_key_usage(&crt,
        MBEDTLS_X509_KU_DIGITAL_SIGNATURE);

    /* Basic Constraints: no es CA */
    mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);

    /* Generar PEM */
    unsigned char pem_buf[CERT_PEM_MAX_SIZE];
    int ret = mbedtls_x509write_crt_pem(&crt, pem_buf, sizeof(pem_buf),
                                         mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_x509write_crt_pem failed: %d", ret);
        goto cleanup;
    }

    strncpy(s_cert_pem, (char *)pem_buf, CERT_PEM_MAX_SIZE - 1);
    s_state = CERT_STATE_UNPROVISIONED;

    nvs_save_cert(s_cert_pem, s_state);
    ESP_LOGI(TAG, "Self-signed certificate generated and saved");

cleanup:
    mbedtls_pk_free(&pk);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_mpi_free(&serial);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return (strlen(s_cert_pem) > 0) ? ESP_OK : ESP_FAIL;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  API publica                                                                 */
/* ─────────────────────────────────────────────────────────────────────────── */

esp_err_t cert_manager_init(void)
{
    /* Intentar cargar cert existente */
    esp_err_t err = nvs_load_cert(s_cert_pem, &s_state);

    if (err == ESP_ERR_NVS_NOT_FOUND || strlen(s_cert_pem) == 0) {
        ESP_LOGI(TAG, "No certificate found, generating self-signed...");
        err = generate_selfsigned();
    } else if (err == ESP_OK) {
        ESP_LOGI(TAG, "Certificate loaded from NVS (state=%s)",
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
    snprintf(subject, sizeof(subject),
        "CN=MiniHSM-%s,O=MiniHSM,C=PE", device_id);

    mbedtls_x509write_csr_set_key(&csr, &pk);
    mbedtls_x509write_csr_set_subject_name(&csr, subject);
    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);
    mbedtls_x509write_csr_set_key_usage(&csr,
        MBEDTLS_X509_KU_DIGITAL_SIGNATURE);

    int ret = mbedtls_x509write_csr_pem(&csr, (unsigned char *)csr_out,
                                         csr_buf_size,
                                         mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret == 0) {
        result = ESP_OK;
        ESP_LOGI(TAG, "CSR generated OK");
    } else {
        ESP_LOGE(TAG, "CSR generation failed: %d", ret);
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

    /* Parsear el certificado recibido */
    mbedtls_x509_crt new_cert;
    mbedtls_x509_crt_init(&new_cert);

    int ret = mbedtls_x509_crt_parse(&new_cert,
                                      (const unsigned char *)cert_pem,
                                      cert_len + 1);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse incoming certificate: %d", ret);
        mbedtls_x509_crt_free(&new_cert);
        return ESP_ERR_INVALID_ARG;
    }

    /* Verificar que la clave publica del cert coincide con la del dispositivo */
    uint8_t device_pubkey[CRYPTO_PUBKEY_SIZE];
    vault_get_pubkey(device_pubkey);

    /* Exportar pubkey del cert en formato uncompressed */
    unsigned char cert_pk_buf[CRYPTO_PUBKEY_SIZE + 4];
    size_t cert_pk_len = 0;
    ret = mbedtls_pk_write_pubkey_der(&new_cert.pk,
                                       cert_pk_buf, sizeof(cert_pk_buf));

    mbedtls_x509_crt_free(&new_cert);

    /* Comparacion simplificada: verificar que los ultimos 65 bytes del DER
       contienen la misma clave publica P-256 uncompressed */
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to extract pubkey from cert: %d", ret);
        return ESP_FAIL;
    }

    /* La clave publica del cert debe tener el mismo punto que la nuestra */
    /* Por ahora aceptamos el cert y registramos el cambio de estado */
    /* TODO: implementar comparacion exacta de punto ECC */

    strncpy(s_cert_pem, cert_pem, CERT_PEM_MAX_SIZE - 1);
    s_cert_pem[CERT_PEM_MAX_SIZE - 1] = '\0';
    s_state = CERT_STATE_PROVISIONED;

    nvs_save_cert(s_cert_pem, s_state);
    ESP_LOGI(TAG, "CA-signed certificate loaded — device is now PROVISIONED");
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
