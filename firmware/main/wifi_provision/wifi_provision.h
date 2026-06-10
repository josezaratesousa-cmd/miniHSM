#pragma once
#include "esp_err.h"
#include <stdbool.h>

#define WIFI_CREDS_SSID_MAX  64
#define WIFI_CREDS_PASS_MAX  64

typedef struct {
    char ssid[WIFI_CREDS_SSID_MAX];
    char pass[WIFI_CREDS_PASS_MAX];
} wifi_creds_t;

typedef enum {
    WIFI_SRC_NVS      = 0,  /* Credenciales guardadas en NVS (provisioning) */
    WIFI_SRC_KCONFIG  = 1,  /* Credenciales en sdkconfig (desarrollo)       */
    WIFI_SRC_NONE     = 2,  /* Sin credenciales — modo provisioning          */
} wifi_creds_source_t;

/**
 * Carga credenciales WiFi con prioridad:
 *   1. NVS (produccion — provisioned)
 *   2. Kconfig CONFIG_WIFI_SSID (desarrollo)
 *   3. Ninguna → modo provisioning
 */
esp_err_t wifi_provision_load(wifi_creds_t *creds_out,
                               wifi_creds_source_t *source_out);

/**
 * Guarda credenciales WiFi en NVS.
 * Llamar desde el endpoint POST /provision/wifi.
 */
esp_err_t wifi_provision_save(const char *ssid, const char *pass);

/**
 * Borra las credenciales WiFi del NVS.
 */
esp_err_t wifi_provision_clear(void);

/**
 * True si hay credenciales en NVS.
 */
bool wifi_provision_has_nvs_creds(void);
