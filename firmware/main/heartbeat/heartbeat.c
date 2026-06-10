#include "heartbeat.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

#include "vault_manager.h"
#include "policy_engine.h"
#include "crypto_engine.h"

static const char *TAG = "heartbeat";

#define HEARTBEAT_STACK_SIZE  4096
#define HEARTBEAT_PRIORITY    5
#define MAX_URL_LEN           512   /* increased from 256 to avoid truncation */
#define MAX_BODY_LEN          512
#define MAX_RESP_LEN          256
#define DEFAULT_INTERVAL_SEC  300   /* 5 minutos */

static char     s_server_url[MAX_URL_LEN];
static uint32_t s_interval_sec  = DEFAULT_INTERVAL_SEC;
static bool     s_running       = false;
static bool     s_registered    = false;
static TaskHandle_t s_task      = NULL;

/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t do_heartbeat(void)
{
    char device_id[17];
    vault_get_device_id(device_id);

    int64_t ts    = esp_timer_get_time() / 1000000LL;
    char    nonce[17];
    snprintf(nonce, sizeof(nonce), "%lld", (long long)(ts ^ 0xC0FFEE));

    /* Generar token HMAC para autenticar el heartbeat */
    char token[65];
    if (policy_generate_token(ts, nonce, token) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate token for heartbeat");
        return ESP_FAIL;
    }

    /* Construir URL: serverUrl/devices/heartbeat */
    char url[MAX_URL_LEN];
    snprintf(url, sizeof(url), "%s/devices/heartbeat", s_server_url);

    /* Body JSON */
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "deviceId",  device_id);
    cJSON_AddStringToObject(body, "token",     token);
    cJSON_AddNumberToObject(body, "timestamp", (double)ts);
    cJSON_AddStringToObject(body, "nonce",     nonce);
    cJSON_AddStringToObject(body, "firmware",  "2.0.0");
    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    /* HTTP POST */
    char resp_buf[MAX_RESP_LEN] = {0};
    (void)resp_buf; /* suppress unused warning — used in read_response below */

    esp_http_client_config_t cfg = {
        .url            = url,
        .method         = HTTP_METHOD_POST,
        .timeout_ms     = 8000,
        .skip_cert_common_name_check = true,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body_str, strlen(body_str));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        esp_http_client_read_response(client, resp_buf, MAX_RESP_LEN - 1);

        if (status == 200) {
            s_registered = true;
            ESP_LOGI(TAG, "Heartbeat OK — device registered at serverHSM");
        } else {
            ESP_LOGW(TAG, "Heartbeat HTTP %d", status);
        }
    } else {
        ESP_LOGW(TAG, "Heartbeat failed: %s", esp_err_to_name(err));
        s_registered = false;
    }

    esp_http_client_cleanup(client);
    free(body_str);
    return err;
}

/* ─────────────────────────────────────────────────────────────────────────── */

static void heartbeat_task(void *arg)
{
    ESP_LOGI(TAG, "Heartbeat task started (interval=%lus, server=%s)",
             (unsigned long)s_interval_sec, s_server_url);

    /* Primer heartbeat inmediato al arrancar */
    vTaskDelay(pdMS_TO_TICKS(5000));
    do_heartbeat();

    while (s_running) {
        vTaskDelay(pdMS_TO_TICKS(s_interval_sec * 1000));
        if (s_running) do_heartbeat();
    }

    ESP_LOGI(TAG, "Heartbeat task stopped");
    s_task = NULL;
    vTaskDelete(NULL);
}

/* ─────────────────────────────────────────────────────────────────────────── */

esp_err_t heartbeat_init(const char *server_url, uint32_t interval_sec)
{
    if (!server_url || strlen(server_url) == 0) return ESP_ERR_INVALID_ARG;

    strncpy(s_server_url, server_url, MAX_URL_LEN - 1);
    s_interval_sec = interval_sec > 0 ? interval_sec : DEFAULT_INTERVAL_SEC;

    ESP_LOGI(TAG, "Heartbeat configured: url=%s interval=%lus",
             s_server_url, (unsigned long)s_interval_sec);
    return ESP_OK;
}

esp_err_t heartbeat_start(void)
{
    if (s_running) return ESP_OK;
    s_running = true;

    BaseType_t ret = xTaskCreate(
        heartbeat_task, "heartbeat",
        HEARTBEAT_STACK_SIZE, NULL,
        HEARTBEAT_PRIORITY, &s_task
    );

    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}

void heartbeat_stop(void)
{
    s_running    = false;
    s_registered = false;
}

bool heartbeat_is_registered(void)
{
    return s_registered;
}
