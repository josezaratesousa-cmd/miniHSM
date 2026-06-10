#include "wifi_provision.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG     = "wifi_provision";
static const char *NVS_NS  = "wifi_creds";
static const char *NVS_SSID= "ssid";
static const char *NVS_PASS= "pass";

/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t nvs_load(char *ssid_out, char *pass_out)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    size_t len = WIFI_CREDS_SSID_MAX;
    err = nvs_get_str(h, NVS_SSID, ssid_out, &len);
    if (err != ESP_OK) goto done;

    len = WIFI_CREDS_PASS_MAX;
    err = nvs_get_str(h, NVS_PASS, pass_out, &len);

done:
    nvs_close(h);
    return err;
}

/* ─────────────────────────────────────────────────────────────────────────── */

esp_err_t wifi_provision_load(wifi_creds_t *creds_out,
                               wifi_creds_source_t *source_out)
{
    memset(creds_out, 0, sizeof(*creds_out));

    /* 1. Intentar NVS primero */
    esp_err_t err = nvs_load(creds_out->ssid, creds_out->pass);
    if (err == ESP_OK && strlen(creds_out->ssid) > 0) {
        ESP_LOGI(TAG, "WiFi credentials from NVS: SSID=%s", creds_out->ssid);
        if (source_out) *source_out = WIFI_SRC_NVS;
        return ESP_OK;
    }

    /* 2. Fallback a Kconfig (desarrollo) */
    const char *kconfig_ssid = CONFIG_WIFI_SSID;
    const char *kconfig_pass = CONFIG_WIFI_PASS;

    if (kconfig_ssid && strlen(kconfig_ssid) > 0) {
        strncpy(creds_out->ssid, kconfig_ssid, WIFI_CREDS_SSID_MAX - 1);
        strncpy(creds_out->pass, kconfig_pass, WIFI_CREDS_PASS_MAX - 1);
        ESP_LOGI(TAG, "WiFi credentials from Kconfig: SSID=%s", creds_out->ssid);
        if (source_out) *source_out = WIFI_SRC_KCONFIG;
        return ESP_OK;
    }

    /* 3. Sin credenciales */
    ESP_LOGW(TAG, "No WiFi credentials found — provisioning mode");
    if (source_out) *source_out = WIFI_SRC_NONE;
    return ESP_ERR_NOT_FOUND;
}

esp_err_t wifi_provision_save(const char *ssid, const char *pass)
{
    if (!ssid || strlen(ssid) == 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_str(h, NVS_SSID, ssid);
    if (err == ESP_OK) err = nvs_set_str(h, NVS_PASS, pass ? pass : "");
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi credentials saved to NVS: SSID=%s", ssid);
    }
    return err;
}

esp_err_t wifi_provision_clear(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_erase_key(h, NVS_SSID);
    nvs_erase_key(h, NVS_PASS);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGW(TAG, "WiFi credentials cleared from NVS");
    return ESP_OK;
}

bool wifi_provision_has_nvs_creds(void)
{
    char ssid[WIFI_CREDS_SSID_MAX] = {0};
    char pass[WIFI_CREDS_PASS_MAX] = {0};
    esp_err_t err = nvs_load(ssid, pass);
    return (err == ESP_OK && strlen(ssid) > 0);
}
