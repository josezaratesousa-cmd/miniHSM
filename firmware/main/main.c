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
#include "heartbeat.h"
#include "match_engine.h"
#include "captive_portal.h"
#include "version.h"
#include "esp_system.h"

static const char *TAG = "main";

/* URL del serverHSM — configurable via menuconfig o sdkconfig */
#ifndef CONFIG_SERVERHSM_URL
#define CONFIG_SERVERHSM_URL "https://api.xami.run"
#endif

#ifndef CONFIG_HEARTBEAT_INTERVAL_SEC
#define CONFIG_HEARTBEAT_INTERVAL_SEC 300
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "=== %s ===", XAMI_VERSION_STRING);

    /* 1. NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS erase needed");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "[1/7] NVS OK");

    /* 2. Crypto Engine */
    ESP_ERROR_CHECK(crypto_engine_init());
    ESP_LOGI(TAG, "[2/7] Crypto Engine OK (PSA / P-256 / DER)");

    /* 3. Vault Manager */
    ESP_ERROR_CHECK(vault_init());
    ESP_LOGI(TAG, "[3/7] Vault Manager OK");

    /* 4. Cert Manager */
    ESP_ERROR_CHECK(cert_manager_init());
    ESP_LOGI(TAG, "[4/7] Cert Manager OK (state=%s)",
        cert_get_state() == CERT_STATE_PROVISIONED ? "PROVISIONED" : "UNPROVISIONED");

    /* 5. Policy Engine */
    ESP_ERROR_CHECK(policy_init());
    ESP_LOGI(TAG, "[5/7] Policy Engine OK");

    /* 6. Audit Engine */
    ESP_ERROR_CHECK(audit_init());
    ESP_LOGI(TAG, "[6/7] Audit Engine OK");

    /* Info del dispositivo */
    char device_id[17];
    vault_get_device_id(device_id);
    ESP_LOGI(TAG, "Device ID : %s", device_id);

    /* 7. WiFi + HTTP server + Heartbeat */
    ret = network_wifi_init();
    if (ret != ESP_OK) {
        /* Sin red: no hay credenciales o fallo la conexion.
         * Levantar el portal Xami para que el usuario configure su WiFi. */
        ESP_LOGW(TAG, "Sin conexion WiFi — abriendo portal de configuracion Xami");
        captive_portal_start();
        /* Si el portal guarda credenciales, reinicia solo (esp_restart).
         * Si hace timeout, reintentamos el ciclo reiniciando. */
        ESP_LOGW(TAG, "Portal cerrado sin configurar — reiniciando para reintentar");
        esp_restart();
    } else {
        ESP_ERROR_CHECK(network_http_server_start());
        network_sntp_start();   /* Fase 0: sincroniza hora (prerequisito TOTP) */

        /* Match: si el device no esta emparejado, se empareja con el server
         * (recibe el HMAC secret cifrado por ECIES). Bloque 9. */
        if (!policy_has_secret()) {
            ESP_LOGI(TAG, "Device sin emparejar — lanzando match en task dedicada...");
            match_start(CONFIG_SERVERHSM_URL);
        }

        /* Heartbeat: registra el miniHSM en el serverHSM cada 5 min */
        heartbeat_init(CONFIG_SERVERHSM_URL, CONFIG_HEARTBEAT_INTERVAL_SEC);
        heartbeat_start();
        ESP_LOGI(TAG, "[7/7] Heartbeat OK (server=%s, interval=%ds)",
                 CONFIG_SERVERHSM_URL, CONFIG_HEARTBEAT_INTERVAL_SEC);
    }

    ESP_LOGI(TAG, "=== Xami listo — Device: %s ===", device_id);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGD(TAG, "ops=%lu registered=%s",
            (unsigned long)audit_get_count(),
            heartbeat_is_registered() ? "YES" : "NO");
    }
}
