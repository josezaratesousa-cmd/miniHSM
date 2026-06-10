#pragma once
#include "esp_err.h"

/**
 * Inicia la tarea de heartbeat.
 * Cada HEARTBEAT_INTERVAL_SEC el miniHSM hace POST al serverHSM
 * con su deviceId + firma HMAC. El serverHSM extrae la IP del request
 * y la guarda. Si la IP cambio, el serverHSM lo sabe automaticamente.
 */
esp_err_t heartbeat_init(const char *server_url, uint32_t interval_sec);
esp_err_t heartbeat_start(void);
void      heartbeat_stop(void);
bool      heartbeat_is_registered(void);
