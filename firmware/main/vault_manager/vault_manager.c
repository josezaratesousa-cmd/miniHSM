#include "vault_manager.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_random.h"

static const char *TAG         = "vault_manager";
static const char *NVS_NS      = "vault";
static const char *NVS_PRIVKEY = "privkey";
static const char *NVS_PUBKEY  = "pubkey";
static const char *NVS_KEKSEC  = "kek_secret";
#define CHIP_KEK_SECRET_SIZE 32

/* Clave publica en RAM (puede estar en memoria, no es secreta) */
static uint8_t s_pubkey[CRYPTO_PUBKEY_SIZE];
static int     s_loaded = 0;

/* -------------------------------------------------------------------------- */

static esp_err_t nvs_save_keypair(const uint8_t *privkey, const uint8_t *pubkey)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(h, NVS_PRIVKEY, privkey, CRYPTO_PRIVKEY_SIZE);
    if (err != ESP_OK) goto done;

    err = nvs_set_blob(h, NVS_PUBKEY, pubkey, CRYPTO_PUBKEY_SIZE);
    if (err != ESP_OK) goto done;

    err = nvs_commit(h);

done:
    nvs_close(h);
    return err;
}

static esp_err_t nvs_load_keypair(uint8_t *privkey_out, uint8_t *pubkey_out)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    size_t len = CRYPTO_PRIVKEY_SIZE;
    err = nvs_get_blob(h, NVS_PRIVKEY, privkey_out, &len);
    if (err != ESP_OK) goto done;

    len = CRYPTO_PUBKEY_SIZE;
    err = nvs_get_blob(h, NVS_PUBKEY, pubkey_out, &len);

done:
    nvs_close(h);
    return err;
}

/* -------------------------------------------------------------------------- */

esp_err_t vault_init(void)
{
    uint8_t privkey[CRYPTO_PRIVKEY_SIZE];
    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];
    esp_err_t err;

    /* Intentar cargar keypair existente */
    err = nvs_load_keypair(privkey, pubkey);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No keypair found, generating new one...");

        err = crypto_generate_keypair(privkey, pubkey);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Keypair generation failed");
            crypto_zeroize(privkey, sizeof(privkey));
            return err;
        }

        err = nvs_save_keypair(privkey, pubkey);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save keypair to NVS");
            crypto_zeroize(privkey, sizeof(privkey));
            return err;
        }

        ESP_LOGI(TAG, "New keypair generated and saved");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS load error: %s", esp_err_to_name(err));
        crypto_zeroize(privkey, sizeof(privkey));
        return err;
    } else {
        ESP_LOGI(TAG, "Keypair loaded from NVS");
    }

    /* Guardar pubkey en RAM, zeroizar privkey */
    memcpy(s_pubkey, pubkey, CRYPTO_PUBKEY_SIZE);
    crypto_zeroize(privkey, sizeof(privkey));
    s_loaded = 1;

    /* Log device ID */
    char device_id[17];
    vault_get_device_id(device_id);
    ESP_LOGI(TAG, "Device ID: %s", device_id);

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */

esp_err_t vault_sign(
    const uint8_t *digest,
    uint8_t       *sig_out,
    size_t        *sig_len_out
) {
    if (!s_loaded) return ESP_ERR_INVALID_STATE;

    /* Cargar privkey desde NVS solo para esta operacion */
    uint8_t privkey[CRYPTO_PRIVKEY_SIZE];
    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];

    esp_err_t err = nvs_load_keypair(privkey, pubkey);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load privkey for signing");
        return err;
    }

    err = crypto_sign(digest, privkey, sig_out, sig_len_out);

    /* Zeroizar inmediatamente */
    crypto_zeroize(privkey, sizeof(privkey));
    crypto_zeroize(pubkey, sizeof(pubkey));

    return err;
}

esp_err_t vault_sign_raw(const uint8_t *digest, uint8_t *raw_out)
{
    if (!s_loaded) return ESP_ERR_INVALID_STATE;
    uint8_t privkey[CRYPTO_PRIVKEY_SIZE];
    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];
    esp_err_t err = nvs_load_keypair(privkey, pubkey);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Failed to load privkey for raw signing"); return err; }
    err = crypto_sign_raw(digest, privkey, raw_out);
    crypto_zeroize(privkey, sizeof(privkey));
    crypto_zeroize(pubkey, sizeof(pubkey));
    return err;
}

/* -------------------------------------------------------------------------- */

esp_err_t vault_get_pubkey(uint8_t *pubkey_out)
{
    if (!s_loaded) return ESP_ERR_INVALID_STATE;
    memcpy(pubkey_out, s_pubkey, CRYPTO_PUBKEY_SIZE);
    return ESP_OK;
}

esp_err_t vault_get_device_id(char *device_id_out)
{
    if (!s_loaded) return ESP_ERR_INVALID_STATE;

    uint8_t hash[CRYPTO_DIGEST_SIZE];
    crypto_sha256(s_pubkey, CRYPTO_PUBKEY_SIZE, hash);

    /* Primeros 8 bytes = 16 hex chars */
    for (int i = 0; i < 8; i++) {
        sprintf(device_id_out + i * 2, "%02x", hash[i]);
    }
    device_id_out[16] = '\0';
    return ESP_OK;
}

esp_err_t vault_destroy_key(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    nvs_erase_key(h, NVS_PRIVKEY);
    nvs_erase_key(h, NVS_PUBKEY);
    nvs_commit(h);
    nvs_close(h);

    crypto_zeroize(s_pubkey, sizeof(s_pubkey));
    s_loaded = 0;

    ESP_LOGW(TAG, "Keypair destroyed");
    return ESP_OK;
}

esp_err_t vault_get_privkey_raw(uint8_t *privkey_out, uint8_t *pubkey_out)
{
    if (!s_loaded) return ESP_ERR_INVALID_STATE;
    return nvs_load_keypair(privkey_out, pubkey_out);
}


/* Fase 0 — secreto local del chip (Opcion 1: aleatorio en NVS).
   Se genera una sola vez y se persiste; en arranques siguientes se reusa.
   xami.run NO lo conoce. Sustituible por eFuse/HMAC en produccion. */
esp_err_t vault_get_chip_kek_secret(uint8_t *secret_out)
{
    if (!secret_out) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    size_t len = CHIP_KEK_SECRET_SIZE;
    err = nvs_get_blob(h, NVS_KEKSEC, secret_out, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND || (err == ESP_OK && len != CHIP_KEK_SECRET_SIZE)) {
        esp_fill_random(secret_out, CHIP_KEK_SECRET_SIZE);   /* primera vez */
        err = nvs_set_blob(h, NVS_KEKSEC, secret_out, CHIP_KEK_SECRET_SIZE);
        if (err == ESP_OK) err = nvs_commit(h);
        ESP_LOGI(TAG, "chip_kek_secret generado y persistido");
    }
    nvs_close(h);
    return err;
}