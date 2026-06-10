#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef enum {
    AUDIT_OP_SIGN    = 0,
    AUDIT_OP_VERIFY  = 1,
    AUDIT_OP_HEALTH  = 2,
    AUDIT_OP_DEVICE  = 3,
    AUDIT_OP_RECONFIG = 4,
} audit_operation_t;

typedef enum {
    AUDIT_RESULT_OK      = 0,
    AUDIT_RESULT_FAIL    = 1,
    AUDIT_RESULT_DENIED  = 2,
} audit_result_t;

/**
 * @brief Inicializa el audit engine.
 */
esp_err_t audit_init(void);

/**
 * @brief Registra una operacion en el audit log.
 *        Cada entrada queda firmada con la clave del vault.
 *
 * @param op            Tipo de operacion
 * @param request_id    UUID del request (string)
 * @param digest_hex    Digest involucrado (puede ser NULL)
 * @param result        Resultado de la operacion
 */
esp_err_t audit_log(
    audit_operation_t  op,
    const char        *request_id,
    const char        *digest_hex,
    audit_result_t     result
);

/**
 * @brief Devuelve el audit log como JSON string.
 *        json_out debe tener suficiente espacio (sugerido: 4096 bytes).
 *        json_len es el tamano del buffer.
 */
esp_err_t audit_get_json(char *json_out, size_t json_len);

/**
 * @brief Devuelve el numero de operaciones registradas.
 */
uint32_t audit_get_count(void);
