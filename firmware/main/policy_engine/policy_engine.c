#include "policy_engine.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "psa/crypto.h"
#include "crypto_engine.h"

static const char *TAG        = "policy_engine";
static const char *NVS_NS     = "policy";
static const char *NVS_SECRET = "hmac_secret";

#define SECRET_SIZE         32
#define TOKEN_WINDOW_SEC    30
#define REPLAY_CACHE_SIZE   32

static uint8_t s_secret[SECRET_SIZE];
static int     s_initialized = 0;
static int     s_has_secret  = 0;

static char s_replay_cache[REPLAY_CACHE_SIZE][65];
static int  s_replay_idx = 0;

/* -------------------------------------------------------------------------- */

static esp_err_t nvs_load_secret(uint8_t *secret_out)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t len = SECRET_SIZE;
    err = nvs_get_blob(h, NVS_SECRET, secret_out, &len);
    nvs_close(h);
    return err;
}

static esp_err_t nvs_save_secret(const uint8_t *secret)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, NVS_SECRET, secret, SECRET_SIZE);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* HMAC-SHA256 usando PSA API */
static esp_err_t compute_hmac(
    const char    *message,
    const uint8_t *secret,
    uint8_t       *hmac_out
) {
    /* Importar clave HMAC */
    psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attrs, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attrs, SECRET_SIZE * 8);
    psa_set_key_usage_flags(&attrs, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attrs, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    psa_key_id_t key_id;
    psa_status_t status = psa_import_key(&attrs, secret, SECRET_SIZE, &key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key HMAC failed: %d", (int)status);
        return ESP_FAIL;
    }

    size_t hmac_len = 0;
    status = psa_mac_compute(
        key_id,
        PSA_ALG_HMAC(PSA_ALG_SHA_256),
        (const uint8_t *)message, strlen(message),
        hmac_out, 32,
        &hmac_len
    );

    psa_destroy_key(key_id);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_mac_compute failed: %d", (int)status);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* -------------------------------------------------------------------------- */

esp_err_t policy_init(void)
{
    esp_err_t err = nvs_load_secret(s_secret);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        /* No hay secret: el device aun no se ha emparejado (match) con el server.
         * El secret llega cifrado (ECIES) en el match y se guarda con policy_set_secret.
         * NO se genera localmente (modelo Bloque 9: el server es la autoridad). */
        ESP_LOGW(TAG, "No HMAC secret — device sin emparejar (pendiente de match)");
        s_has_secret = 0;
    } else if (err != ESP_OK) {
        return err;
    } else {
        ESP_LOGI(TAG, "HMAC secret loaded from NVS");
        s_has_secret = 1;
    }

    memset(s_replay_cache, 0, sizeof(s_replay_cache));
    s_initialized = 1;
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */

esp_err_t policy_validate_token(const char *token, int64_t timestamp, const char *nonce)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (!token || strlen(token) != 64) return ESP_ERR_INVALID_ARG;

    /* 1. Verificar ventana de tiempo */
    int64_t now_sec = esp_timer_get_time() / 1000000LL;
    int64_t diff = now_sec - timestamp;
    if (diff < -TOKEN_WINDOW_SEC || diff > TOKEN_WINDOW_SEC) {
        ESP_LOGW(TAG, "Token timestamp out of window (diff=%lld)", (long long)diff);
        return ESP_ERR_INVALID_ARG;
    }

    /* 2. Recalcular HMAC esperado */
    char message[128];
    snprintf(message, sizeof(message), "minihsm:%lld:%s", (long long)timestamp, nonce);

    uint8_t expected_hmac[32];
    if (compute_hmac(message, s_secret, expected_hmac) != ESP_OK) return ESP_FAIL;

    char expected_hex[65];
    crypto_bytes_to_hex(expected_hmac, 32, expected_hex);

    /* 3. Comparacion constante */
    int mismatch = 0;
    for (int i = 0; i < 64; i++) {
        mismatch |= (token[i] ^ expected_hex[i]);
    }
    if (mismatch != 0) {
        ESP_LOGW(TAG, "Token HMAC mismatch");
        return ESP_ERR_INVALID_ARG;
    }

    /* 4. Anti-replay */
    for (int i = 0; i < REPLAY_CACHE_SIZE; i++) {
        if (strncmp(s_replay_cache[i], token, 64) == 0) {
            ESP_LOGW(TAG, "Replay attack detected");
            return ESP_ERR_INVALID_ARG;
        }
    }

    /* 5. Marcar token como usado */
    strncpy(s_replay_cache[s_replay_idx], token, 64);
    s_replay_cache[s_replay_idx][64] = '\0';
    s_replay_idx = (s_replay_idx + 1) % REPLAY_CACHE_SIZE;

    return ESP_OK;
}

esp_err_t policy_generate_token(int64_t timestamp, const char *nonce, char *token_out)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    char message[128];
    snprintf(message, sizeof(message), "minihsm:%lld:%s", (long long)timestamp, nonce);

    uint8_t hmac[32];
    if (compute_hmac(message, s_secret, hmac) != ESP_OK) return ESP_FAIL;
    crypto_bytes_to_hex(hmac, 32, token_out);
    return ESP_OK;
}

esp_err_t policy_get_secret_hex(char *secret_out)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    crypto_bytes_to_hex(s_secret, SECRET_SIZE, secret_out);
    return ESP_OK;
}
/* -------------------------------------------------------------------------- */
/* Provisioning del secret via match (Bloque 9)                               */

esp_err_t policy_set_secret(const uint8_t *secret)
{
    /* Guarda el HMAC secret recibido del server (descifrado por ECIES en el match). */
    esp_err_t err = nvs_save_secret(secret);
    if (err != ESP_OK) return err;
    memcpy(s_secret, secret, SECRET_SIZE);
    s_has_secret = 1;
    ESP_LOGI(TAG, "HMAC secret provisionado via match y guardado en NVS");
    return ESP_OK;
}

int policy_has_secret(void)
{
    return s_has_secret;
}
