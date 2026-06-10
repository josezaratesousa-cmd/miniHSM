#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "crypto_engine.h"
#include "vault_manager.h"
#include "cert_manager.h"
#include "policy_engine.h"
#include "audit_engine.h"
#include "network_engine.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== MiniHSM v2.0.0 ===");

    /* 1. NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS erase needed");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "[1/6] NVS OK");

    /* 2. Crypto Engine */
    ESP_ERROR_CHECK(crypto_engine_init());
    ESP_LOGI(TAG, "[2/6] Crypto Engine OK (PSA / P-256 / DER)");

    /* 3. Vault Manager */
    ESP_ERROR_CHECK(vault_init());
    ESP_LOGI(TAG, "[3/6] Vault Manager OK");

    /* 4. Cert Manager */
    ESP_ERROR_CHECK(cert_manager_init());
    ESP_LOGI(TAG, "[4/6] Cert Manager OK (state=%s)",
        cert_get_state() == CERT_STATE_PROVISIONED ? "PROVISIONED" : "UNPROVISIONED");

    /* 5. Policy Engine */
    ESP_ERROR_CHECK(policy_init());
    ESP_LOGI(TAG, "[5/6] Policy Engine OK");

    /* 6. Audit Engine */
    ESP_ERROR_CHECK(audit_init());
    ESP_LOGI(TAG, "[6/6] Audit Engine OK");

    /* Info del dispositivo */
    char device_id[17];
    vault_get_device_id(device_id);
    ESP_LOGI(TAG, "Device ID : %s", device_id);

    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];
    char    pubkey_hex[CRYPTO_PUBKEY_SIZE * 2 + 1];
    vault_get_pubkey(pubkey);
    crypto_bytes_to_hex(pubkey, CRYPTO_PUBKEY_SIZE, pubkey_hex);
    ESP_LOGI(TAG, "Public Key: %.32s...", pubkey_hex);

    char fingerprint[65];
    cert_get_fingerprint(fingerprint);
    ESP_LOGI(TAG, "Cert SHA256: %.16s...", fingerprint);

    /* WiFi + HTTP */
    ret = network_wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi failed — running without network");
    } else {
        ESP_ERROR_CHECK(network_http_server_start());
    }

    ESP_LOGI(TAG, "=== MiniHSM ready ===");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGD(TAG, "Heartbeat ops=%lu state=%s",
            (unsigned long)audit_get_count(),
            cert_get_state() == CERT_STATE_PROVISIONED ? "PROV" : "DEV");
    }
}
