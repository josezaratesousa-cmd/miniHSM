#include "audit_engine.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "vault_manager.h"
#include "crypto_engine.h"

static const char *TAG = "audit_engine";

#define AUDIT_LOG_SIZE  64   /* Circular buffer en RAM */

typedef struct {
    int64_t     timestamp_ms;
    uint8_t     operation;
    char        request_id[37];    /* UUID */
    char        digest_hex[65];    /* SHA-256 hex o vacio */
    uint8_t     result;
    char        device_id[17];
    uint8_t     sig[CRYPTO_SIG_DER_MAX_SIZE];
    size_t      sig_len;
    int         valid;
} audit_entry_t;

static audit_entry_t s_log[AUDIT_LOG_SIZE];
static int           s_log_idx   = 0;
static uint32_t      s_log_count = 0;
static int           s_initialized = 0;

/* -------------------------------------------------------------------------- */

static const char *op_to_str(audit_operation_t op)
{
    switch (op) {
        case AUDIT_OP_SIGN:   return "sign";
        case AUDIT_OP_VERIFY: return "verify";
        case AUDIT_OP_HEALTH: return "health";
        case AUDIT_OP_DEVICE: return "device";
        default:              return "unknown";
    }
}

static const char *result_to_str(audit_result_t r)
{
    switch (r) {
        case AUDIT_RESULT_OK:     return "ok";
        case AUDIT_RESULT_FAIL:   return "fail";
        case AUDIT_RESULT_DENIED: return "denied";
        default:                  return "unknown";
    }
}

/* -------------------------------------------------------------------------- */

esp_err_t audit_init(void)
{
    memset(s_log, 0, sizeof(s_log));
    s_log_idx     = 0;
    s_log_count   = 0;
    s_initialized = 1;
    ESP_LOGI(TAG, "Audit engine initialized (RAM buffer, size=%d)", AUDIT_LOG_SIZE);
    return ESP_OK;
}

esp_err_t audit_log(
    audit_operation_t  op,
    const char        *request_id,
    const char        *digest_hex,
    audit_result_t     result
) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    audit_entry_t *entry = &s_log[s_log_idx];
    memset(entry, 0, sizeof(audit_entry_t));

    entry->timestamp_ms = esp_timer_get_time() / 1000LL;
    entry->operation    = (uint8_t)op;
    entry->result       = (uint8_t)result;

    strncpy(entry->request_id, request_id ? request_id : "unknown", 36);

    if (digest_hex) {
        strncpy(entry->digest_hex, digest_hex, 64);
    }

    vault_get_device_id(entry->device_id);

    /* Construir mensaje a firmar */
    char msg[256];
    snprintf(msg, sizeof(msg), "%lld:%s:%s:%s:%s",
        (long long)entry->timestamp_ms,
        entry->device_id,
        op_to_str(op),
        entry->request_id,
        result_to_str(result)
    );

    /* Firmar el hash del mensaje */
    uint8_t hash[CRYPTO_DIGEST_SIZE];
    crypto_sha256((const uint8_t *)msg, strlen(msg), hash);
    vault_sign(hash, entry->sig, &entry->sig_len);
    entry->valid = 1;

    s_log_idx = (s_log_idx + 1) % AUDIT_LOG_SIZE;
    s_log_count++;

    ESP_LOGD(TAG, "Audit: op=%s req=%s result=%s",
        op_to_str(op), entry->request_id, result_to_str(result));

    return ESP_OK;
}

esp_err_t audit_get_json(char *json_out, size_t json_len)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    int filled = snprintf(json_out, json_len, "{\"count\":%lu,\"entries\":[", (unsigned long)s_log_count);

    int total_entries = (s_log_count < AUDIT_LOG_SIZE) ? s_log_count : AUDIT_LOG_SIZE;
    int first = 1;

    for (int i = 0; i < total_entries; i++) {
        /* Iterar desde el mas antiguo al mas nuevo */
        int idx = (s_log_idx - total_entries + i + AUDIT_LOG_SIZE) % AUDIT_LOG_SIZE;
        audit_entry_t *e = &s_log[idx];
        if (!e->valid) continue;

        char sig_hex[CRYPTO_SIG_DER_MAX_SIZE * 2 + 1];
        crypto_bytes_to_hex(e->sig, e->sig_len, sig_hex);

        char entry_json[512];
        int entry_len = snprintf(entry_json, sizeof(entry_json),
            "%s{\"ts\":%lld,\"op\":\"%s\",\"reqId\":\"%s\",\"result\":\"%s\",\"deviceId\":\"%s\",\"sig\":\"%s\"}",
            first ? "" : ",",
            (long long)e->timestamp_ms,
            op_to_str((audit_operation_t)e->operation),
            e->request_id,
            result_to_str((audit_result_t)e->result),
            e->device_id,
            sig_hex
        );

        if (filled + entry_len + 2 < (int)json_len) {
            memcpy(json_out + filled, entry_json, entry_len);
            filled += entry_len;
            first = 0;
        }
    }

    snprintf(json_out + filled, json_len - filled, "]}");
    return ESP_OK;
}

uint32_t audit_get_count(void)
{
    return s_log_count;
}
