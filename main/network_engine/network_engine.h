#pragma once

#include "esp_err.h"

/**
 * @brief Inicializa WiFi en modo Station y se conecta.
 *        SSID y password se leen de sdkconfig (menuconfig) o
 *        pueden sobreescribirse con las macros WIFI_SSID / WIFI_PASS.
 */
esp_err_t network_wifi_init(void);

/**
 * @brief Inicia el servidor HTTP con todos los endpoints de la API.
 *        Llamar despues de network_wifi_init().
 */
esp_err_t network_http_server_start(void);

/**
 * @brief Detiene el servidor HTTP.
 */
esp_err_t network_http_server_stop(void);
