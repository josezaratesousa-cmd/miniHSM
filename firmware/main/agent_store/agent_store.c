/* agent_store — R pendiente de entrega + nonce anti-replay por slot (NVS). */
#include "agent_store.h"
#include <string.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG    = "agent_store";
static const char *NVS_NS = "agentst";
#define PEND_SIZE 64   /* fingerprint(32) || R(32) */

/* key corta por slot: "<pre><slot>" (pre: "p"=pend, "n"=nonce). */
static void key_for(char *out, char pre, int slot){
    snprintf(out, 12, "%c%d", pre, slot);
}

esp_err_t agent_store_init(void){
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK){ ESP_LOGE(TAG, "nvs_open err=0x%x", err); return err; }
    nvs_close(h);
    ESP_LOGI(TAG, "agent_store OK (R pendiente + nonce anti-replay)");
    return ESP_OK;
}

esp_err_t agent_pending_set(int slot, const uint8_t fp[32], const uint8_t r[32]){
    if (slot < 0 || !fp || !r) return ESP_ERR_INVALID_ARG;
    uint8_t blob[PEND_SIZE];
    memcpy(blob, fp, 32);
    memcpy(blob + 32, r, 32);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    char k[12];
    key_for(k, 'p', slot); err = nvs_set_blob(h, k, blob, PEND_SIZE);
    key_for(k, 'n', slot); if (err==ESP_OK) err = nvs_set_u64(h, k, 0);  /* nonce reset */
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    memset(blob, 0, sizeof(blob));
    if (err == ESP_OK) ESP_LOGI(TAG, "R pendiente retenida (slot %d)", slot);
    return err;
}

esp_err_t agent_pending_get(int slot, uint8_t fp[32], uint8_t r[32]){
    if (slot < 0 || !fp || !r) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    uint8_t blob[PEND_SIZE]; size_t len = PEND_SIZE;
    char k[12]; key_for(k, 'p', slot);
    err = nvs_get_blob(h, k, blob, &len);
    nvs_close(h);
    if (err != ESP_OK) return err;
    if (len != PEND_SIZE) return ESP_ERR_INVALID_SIZE;
    memcpy(fp, blob, 32);
    memcpy(r,  blob + 32, 32);
    memset(blob, 0, sizeof(blob));
    return ESP_OK;
}

esp_err_t agent_pending_clear(int slot){
    if (slot < 0) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    char k[12]; key_for(k, 'p', slot);
    err = nvs_erase_key(h, k);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;  /* idempotente */
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) ESP_LOGI(TAG, "R olvidada (slot %d)", slot);
    return err;
}

bool agent_pending_has(int slot){
    if (slot < 0) return false;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    uint8_t blob[PEND_SIZE]; size_t len = PEND_SIZE;
    char k[12]; key_for(k, 'p', slot);
    esp_err_t err = nvs_get_blob(h, k, blob, &len);
    nvs_close(h);
    memset(blob, 0, sizeof(blob));
    return err == ESP_OK;
}

static uint64_t nonce_last(int slot){
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return 0;
    uint64_t v = 0; char k[12]; key_for(k, 'n', slot);
    if (nvs_get_u64(h, k, &v) != ESP_OK) v = 0;
    nvs_close(h);
    return v;
}

bool agent_nonce_ok(int slot, uint64_t nonce){
    if (slot < 0) return false;
    return nonce > nonce_last(slot);   /* estrictamente creciente */
}

esp_err_t agent_nonce_commit(int slot, uint64_t nonce){
    if (slot < 0) return ESP_ERR_INVALID_ARG;
    if (nonce <= nonce_last(slot)) return ESP_ERR_INVALID_STATE;  /* no retroceder */
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    char k[12]; key_for(k, 'n', slot);
    err = nvs_set_u64(h, k, nonce);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
