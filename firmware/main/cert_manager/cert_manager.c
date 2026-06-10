#include "cert_manager.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "crypto_engine.h"
#include "vault_manager.h"

/* mbedTLS 4.x (ESP-IDF 6.x): las funciones mbedtls_x509write_* fueron
 * eliminadas del API publico en esta version. La generacion de certificados
 * X.509 debe hacerse externamente (Python/OpenSSL) y cargarse via POST /cert.
 * Este modulo solo gestiona almacenamiento y lectura del certificado. */
#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"

static const char *TAG       = "cert_manager";
static const char *NVS_NS    = "cert";
static const char *NVS_PEM   = "cert_pem";
static const char *NVS_STATE = "state";

/* Certificado autofirmado placeholder — se reemplaza via POST /cert */
#define PLACEHOLDER_CERT \
    "-----BEGIN CERTIFICATE-----\n" \
    "PLACEHOLDER-load-cert-via-POST-/cert\n" \
    "-----END CERTIFICATE-----\n"

static char         s_cert_pem[CERT_PEM_MAX_SIZE];
static cert_state_t s_state       = CERT_STATE_UNPROVISIONED;
static int          s_initialized = 0;

/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */

esp_err_t cert_manager_init(void)
{
    esp_err_t err = nvs_load_cert(s_cert_pem, &s_state);

    if (err == ESP_ERR_NVS_NOT_FOUND || strlen(s_cert_pem) == 0) {
        /* No hay cert — cargar placeholder hasta que se provisione via POST /cert */
        strncpy(s_cert_pem, PLACEHOLDER_CERT, CERT_PEM_MAX_SIZE - 1);
        s_state = CERT_STATE_UNPROVISIONED;
        nvs_save_cert(s_cert_pem, s_state);
        ESP_LOGI(TAG, "No cert found — placeholder loaded. Use POST /cert to provision.");
    } else if (err == ESP_OK) {
        ESP_LOGI(TAG, "Cert loaded (state=%s)",
            s_state == CERT_STATE_PROVISIONED ? "PROVISIONED" : "UNPROVISIONED");
    } else {
        ESP_LOGE(TAG, "NVS error: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = 1;
    return ESP_OK;
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
    /* CSR generation requires x509write which is not available in this mbedTLS version.
       Generate the CSR externally using the device pubkey:
         openssl req -new -key <(openssl ec -inform DER ...) -out device.csr
       Then sign it with your CA and load via POST /cert */
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];
    vault_get_pubkey(pubkey);
    char pubkey_hex[CRYPTO_PUBKEY_SIZE * 2 + 1];
    crypto_bytes_to_hex(pubkey, CRYPTO_PUBKEY_SIZE, pubkey_hex);

    snprintf(csr_out, csr_buf_size,
        "-----BEGIN CERTIFICATE REQUEST-----\n"
        "CSR generation not supported in firmware.\n"
        "Use device pubkey to generate CSR externally:\n"
        "pubkey=%s\n"
        "-----END CERTIFICATE REQUEST-----\n",
        pubkey_hex);

    ESP_LOGI(TAG, "CSR: use device pubkey %s to generate CSR externally", pubkey_hex);
    return ESP_OK;
}

esp_err_t cert_load_ca_signed(const char *cert_pem, size_t cert_len)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    /* Validar que es un PEM valido parseandolo */
    mbedtls_x509_crt c;
    mbedtls_x509_crt_init(&c);
    int ret = mbedtls_x509_crt_parse(&c,
                                      (const unsigned char *)cert_pem,
                                      cert_len + 1);
    mbedtls_x509_crt_free(&c);

    if (ret != 0) {
        ESP_LOGE(TAG, "Invalid certificate: -0x%04X", (unsigned)-ret);
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_cert_pem, cert_pem, CERT_PEM_MAX_SIZE - 1);
    s_cert_pem[CERT_PEM_MAX_SIZE - 1] = '\0';
    s_state = CERT_STATE_PROVISIONED;
    nvs_save_cert(s_cert_pem, s_state);
    ESP_LOGI(TAG, "Certificate loaded — PROVISIONED");
    return ESP_OK;
}

cert_state_t cert_get_state(void) { return s_state; }

esp_err_t cert_get_fingerprint(char *fingerprint_out)
{
    if (!s_initialized || strlen(s_cert_pem) == 0) return ESP_ERR_INVALID_STATE;
    uint8_t hash[CRYPTO_DIGEST_SIZE];
    crypto_sha256((const uint8_t *)s_cert_pem, strlen(s_cert_pem), hash);
    crypto_bytes_to_hex(hash, CRYPTO_DIGEST_SIZE, fingerprint_out);
    return ESP_OK;
}
