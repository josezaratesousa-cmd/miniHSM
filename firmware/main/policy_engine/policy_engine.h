#pragma once

#include "esp_err.h"

/**
 * @brief Inicializa el policy engine.
 *        Carga o genera el secreto HMAC para validacion de tokens.
 */
esp_err_t policy_init(void);

/**
 * @brief Valida un KUser token.
 *        El token es un HMAC-SHA256 de "minihsm:{timestamp}:{nonce}"
 *        usando el secreto del dispositivo.
 *
 *        Validaciones:
 *        - HMAC correcto
 *        - Timestamp dentro de ventana de 30 segundos
 *        - Token no usado antes (anti-replay, lista en RAM)
 *
 * @param token     Token hex (64 chars = 32 bytes HMAC)
 * @param timestamp Unix timestamp del request
 * @param nonce     Nonce del request
 * @return ESP_OK si autorizado, ESP_ERR_NOT_ALLOWED si rechazado
 */
esp_err_t policy_validate_token(const char *token, int64_t timestamp, const char *nonce);

/**
 * @brief Genera un token valido (util para testing y provisioning inicial).
 *        token_out debe tener al menos 65 bytes.
 */
esp_err_t policy_generate_token(int64_t timestamp, const char *nonce, char *token_out);

/**
 * @brief Devuelve el secreto HMAC en hex (solo para setup inicial).
 *        secret_out debe tener al menos 65 bytes.
 */
esp_err_t policy_get_secret_hex(char *secret_out);

/* Provisioning del secret via match (Bloque 9) */
esp_err_t policy_set_secret(const uint8_t *secret);  /* secret de SECRET_SIZE (32) bytes */
int       policy_has_secret(void);
