#pragma once
#include "esp_err.h"
#include <stdbool.h>

/* Portal de configuracion Xami (SoftAP + captive portal).
 *
 * Se activa cuando:
 *   - Primer arranque sin credenciales en NVS
 *   - Fallo de conexion a la red guardada (fallback)
 *   - Reconfiguracion remota solicitada
 *
 * Levanta un AP abierto "Xami-<DeviceID>", sirve una pagina web de
 * bienvenida donde el usuario elige su red WiFi y escribe la contrasena.
 * Al guardar credenciales validas, el dispositivo reinicia en modo STA.
 *
 * El portal se autocierra tras CAPTIVE_PORTAL_TIMEOUT_SEC de inactividad. */

#define CAPTIVE_PORTAL_TIMEOUT_SEC  600   /* 10 minutos */

/**
 * Arranca el SoftAP + portal web + DNS captivo.
 * Bloquea hasta que se guarden credenciales (luego reinicia) o
 * hasta el timeout (luego retorna ESP_ERR_TIMEOUT).
 */
esp_err_t captive_portal_start(void);

/**
 * Detiene el portal y libera recursos.
 */
esp_err_t captive_portal_stop(void);
