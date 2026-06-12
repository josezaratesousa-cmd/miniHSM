#include "heartbeat.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

#include "vault_manager.h"
#include "policy_engine.h"
#include "crypto_engine.h"
#include "cert_manager.h"
#include "custody_manager.h"
#include "match_engine.h"
#include "version.h"

static const char *TAG = "heartbeat";

/* Stack 8192 (antes 4096): el heartbeat ahora puede FIRMAR inline (vault_sign +
   TLS del POST de resultado). 8192 es el numero ya probado-seguro en el match. */
#define HEARTBEAT_STACK_SIZE  8192
#define HEARTBEAT_PRIORITY    5
#define MAX_URL_LEN           512
#define MAX_BODY_LEN          512
/* 768 (antes 256): cabe el job (digest 64 + kuser 64 + nonce + ts + requestId). */
#define MAX_RESP_LEN          768
#define DEFAULT_INTERVAL_SEC  300

static char     s_server_url[MAX_URL_LEN];
static uint32_t s_interval_sec = DEFAULT_INTERVAL_SEC;
static bool     s_running      = false;
static bool     s_registered   = false;
static TaskHandle_t s_task     = NULL;

/* -------------------------------------------------------------------------- */
/* Bloque 10: postear el resultado de un job de firma de vuelta al server.     */

static void post_job_result(const char *request_id, const char *device_id,
                             const char *sig_hex, const char *cert_pem,
                             const char *status, const char *error)
{
    static char url[640];
    snprintf(url, sizeof(url), "%s/devices/%s/jobs/%s/result",
             s_server_url, device_id, request_id);

    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "signature", sig_hex);
    cJSON_AddStringToObject(b, "cert",      cert_pem);
    cJSON_AddStringToObject(b, "status",    status);
    cJSON_AddStringToObject(b, "error",     error);
    char *bj = cJSON_PrintUnformatted(b);
    cJSON_Delete(b);

    esp_http_client_config_t cfg = {
        .url               = url,
        .method            = HTTP_METHOD_POST,
        .timeout_ms        = 8000,
        .transport_type    = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t cl = esp_http_client_init(&cfg);
    esp_http_client_set_header(cl, "Content-Type", "application/json");
    esp_http_client_set_post_field(cl, bj, strlen(bj));

    esp_err_t e = esp_http_client_open(cl, strlen(bj));
    if (e == ESP_OK) {
        esp_http_client_write(cl, bj, strlen(bj));
        esp_http_client_fetch_headers(cl);
        int st = esp_http_client_get_status_code(cl);
        ESP_LOGI(TAG, "result POST %s -> HTTP %d", request_id, st);
    } else {
        ESP_LOGW(TAG, "result POST open failed: %s", esp_err_to_name(e));
    }
    esp_http_client_cleanup(cl);
    free(bj);
}

/* Procesa un job de firma recibido en la respuesta del heartbeat. */
static void process_signing_job(cJSON *job)
{
    const char *rid   = cJSON_GetStringValue(cJSON_GetObjectItem(job, "requestId"));
    const char *dhex  = cJSON_GetStringValue(cJSON_GetObjectItem(job, "digest"));
    const char *kuser = cJSON_GetStringValue(cJSON_GetObjectItem(job, "kuser"));
    cJSON      *tsit  = cJSON_GetObjectItem(job, "ts");
    const char *nonce = cJSON_GetStringValue(cJSON_GetObjectItem(job, "nonce"));

    if (!rid || !dhex || !kuser || !tsit || !nonce || strlen(dhex) != 64) {
        ESP_LOGW(TAG, "job mal formado, ignorado");
        return;
    }

    char device_id[17];
    vault_get_device_id(device_id);
    int64_t ts = (int64_t)cJSON_GetNumberValue(tsit);

    ESP_LOGI(TAG, "job %s recibido, validando token", rid);
    if (policy_validate_token(kuser, ts, nonce) != ESP_OK) {
        ESP_LOGW(TAG, "job %s: token invalido/expirado", rid);
        post_job_result(rid, device_id, "", "", "ERROR", "invalid or expired token");
        return;
    }

    uint8_t digest[CRYPTO_DIGEST_SIZE];
    if (crypto_hex_to_bytes(dhex, digest, CRYPTO_DIGEST_SIZE) != ESP_OK) {
        post_job_result(rid, device_id, "", "", "ERROR", "bad digest hex");
        return;
    }

    uint8_t sig_der[CRYPTO_SIG_DER_MAX_SIZE];
    size_t  sig_len = 0;
    static char cert_pem[CERT_PEM_MAX_SIZE];   /* cabe cert del device y custodiado */

    /* Fase 2: credencial custodiada (slot) vs clave de iniciacion del device */
    cJSON      *cidit   = cJSON_GetObjectItem(job, "credentialId");
    const char *authhex = cJSON_GetStringValue(cJSON_GetObjectItem(job, "auth"));

    if (cidit && cJSON_IsNumber(cidit)) {
        int slot = (int)cJSON_GetNumberValue(cidit);
        if (!authhex) {
            post_job_result(rid, device_id, "", "", "ERROR", "custody: missing auth");
            return;
        }
        /* auth = blob ECIES (hex) con la passphrase cifrada hacia la pubkey del device.
           TODO: domain-separar el info HKDF del de match (hoy reutiliza match_ecies_decrypt). */
        size_t ablen = strlen(authhex) / 2;
        uint8_t *authblob = malloc(ablen ? ablen : 1);
        uint8_t pass[128]; size_t pass_len = 0;
        esp_err_t aerr = (authblob && crypto_hex_to_bytes(authhex, authblob, ablen) == ESP_OK)
                         ? match_ecies_decrypt(authblob, ablen, pass, sizeof(pass), &pass_len)
                         : ESP_FAIL;
        if (authblob) free(authblob);
        if (aerr != ESP_OK) {
            crypto_zeroize(pass, sizeof(pass));
            post_job_result(rid, device_id, "", "", "ERROR", "custody: auth decrypt failed");
            return;
        }
        esp_err_t serr = custody_sign(slot, pass, pass_len, digest, sig_der, &sig_len);
        crypto_zeroize(pass, sizeof(pass));
        if (serr != ESP_OK) {
            ESP_LOGE(TAG, "job %s: custody_sign slot %d fallo (%d)", rid, slot, (int)serr);
            post_job_result(rid, device_id, "", "", "ERROR", "custody sign failed");
            return;
        }
        if (custody_get_cert(slot, cert_pem, sizeof(cert_pem)) != ESP_OK) {
            post_job_result(rid, device_id, "", "", "ERROR", "custody: cert not found");
            return;
        }
    } else {
        if (vault_sign(digest, sig_der, &sig_len) != ESP_OK) {
            ESP_LOGE(TAG, "job %s: vault_sign fallo", rid);
            post_job_result(rid, device_id, "", "", "ERROR", "sign failed");
            return;
        }
        cert_get_pem(cert_pem, sizeof(cert_pem));
    }

    static char sig_hex[CRYPTO_SIG_DER_MAX_SIZE * 2 + 1];
    crypto_bytes_to_hex(sig_der, sig_len, sig_hex);

    ESP_LOGI(TAG, "job %s FIRMADO (sig_len=%d), posteando resultado", rid, (int)sig_len);
    post_job_result(rid, device_id, sig_hex, cert_pem, "DONE", "");
}

/* -------------------------------------------------------------------------- */

static esp_err_t do_heartbeat(void)
{
    char device_id[17];
    vault_get_device_id(device_id);

    int64_t ts = esp_timer_get_time() / 1000000LL;
    char nonce[17];
    snprintf(nonce, sizeof(nonce), "%lld", (long long)(ts ^ 0xC0FFEE));

    char token[65];
    if (policy_generate_token(ts, nonce, token) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate token");
        return ESP_FAIL;
    }

    char url[MAX_URL_LEN];
    int url_len = snprintf(url, sizeof(url), "%.*s/devices/heartbeat",
                           (int)(MAX_URL_LEN - 20), s_server_url);
    if (url_len <= 0 || url_len >= (int)sizeof(url)) {
        ESP_LOGE(TAG, "URL too long");
        return ESP_FAIL;
    }

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "deviceId",  device_id);
    cJSON_AddStringToObject(body, "token",     token);
    cJSON_AddNumberToObject(body, "timestamp", (double)ts);
    cJSON_AddStringToObject(body, "nonce",     nonce);
    cJSON_AddStringToObject(body, "firmware",  XAMI_VERSION);
    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    static char resp_buf[MAX_RESP_LEN];
    resp_buf[0] = 0;

    esp_http_client_config_t cfg = {
        .url               = url,
        .method            = HTTP_METHOD_POST,
        .timeout_ms        = 8000,
        .transport_type    = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body_str, strlen(body_str));

    /* Patron manual open/write/read (igual que el match): perform() consume el
       stream y read_response() puede devolver vacio. */
    int status = 0, rlen = 0;
    esp_err_t err = esp_http_client_open(client, strlen(body_str));
    if (err == ESP_OK) {
        esp_http_client_write(client, body_str, strlen(body_str));
        esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        rlen = esp_http_client_read(client, resp_buf, MAX_RESP_LEN - 1);
        if (rlen < 0) rlen = 0;
        resp_buf[rlen] = 0;
    } else {
        ESP_LOGW(TAG, "Heartbeat open failed: %s", esp_err_to_name(err));
        s_registered = false;
    }

    /* Cerrar el cliente del heartbeat ANTES de firmar/postear (1 solo TLS a la vez). */
    esp_http_client_cleanup(client);
    free(body_str);

    if (err == ESP_OK && status == 200) {
        s_registered = true;
        ESP_LOGI(TAG, "Heartbeat OK (resp_len=%d)", rlen);

        cJSON *resp = cJSON_Parse(resp_buf);
        if (resp) {
            /* Ritmo de poll dictado por el server -> a RAM. */
            cJSON *np = cJSON_GetObjectItem(resp, "nextPollSeconds");
            if (cJSON_IsNumber(np) && np->valuedouble > 0) {
                uint32_t v = (uint32_t)np->valuedouble;
                if (v != s_interval_sec) {
                    ESP_LOGI(TAG, "nextPollSeconds: %" PRIu32 " -> %" PRIu32, s_interval_sec, v);
                    s_interval_sec = v;
                }
            }
            /* Trabajo de firma pendiente? */
            cJSON *job = cJSON_GetObjectItem(resp, "job");
            if (job && cJSON_IsObject(job)) {
                process_signing_job(job);
            }
            cJSON_Delete(resp);
        } else if (rlen > 0) {
            ESP_LOGW(TAG, "Heartbeat resp no es JSON: %.80s", resp_buf);
        }
    } else if (err == ESP_OK) {
        ESP_LOGW(TAG, "Heartbeat HTTP %d", status);
    }

    return err;
}

/* -------------------------------------------------------------------------- */

static void heartbeat_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(5000));
    do_heartbeat();
    while (s_running) {
        vTaskDelay(pdMS_TO_TICKS(s_interval_sec * 1000));
        if (s_running) do_heartbeat();
    }
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t heartbeat_init(const char *server_url, uint32_t interval_sec)
{
    if (!server_url || strlen(server_url) == 0) return ESP_ERR_INVALID_ARG;
    strncpy(s_server_url, server_url, MAX_URL_LEN - 20);
    s_server_url[MAX_URL_LEN - 20] = '\0';
    s_interval_sec = interval_sec > 0 ? interval_sec : DEFAULT_INTERVAL_SEC;
    return ESP_OK;
}

esp_err_t heartbeat_start(void)
{
    if (s_running) return ESP_OK;
    s_running = true;
    BaseType_t ret = xTaskCreate(heartbeat_task, "heartbeat",
                                  HEARTBEAT_STACK_SIZE, NULL,
                                  HEARTBEAT_PRIORITY, &s_task);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}

void heartbeat_stop(void)  { s_running = false; s_registered = false; }
bool heartbeat_is_registered(void) { return s_registered; }
