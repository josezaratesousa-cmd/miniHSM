#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define CERT_PEM_MAX_SIZE   2048
#define CERT_CSR_MAX_SIZE   1024
#define CERT_SUBJECT_MAX    128

typedef enum {
    CERT_STATE_UNPROVISIONED = 0,  /* Solo tiene cert autofirmado */
    CERT_STATE_PROVISIONED   = 1,  /* Tiene cert firmado por CA   */
} cert_state_t;

/**
 * Inicializa el cert_manager.
 * Si no hay cert en NVS genera uno autofirmado.
 */
esp_err_t cert_manager_init(void);

/**
 * Devuelve el certificado actual en PEM.
 * buf debe tener al menos CERT_PEM_MAX_SIZE bytes.
 */
esp_err_t cert_get_pem(char *pem_out, size_t pem_buf_size);

/**
 * Genera y devuelve un CSR PKCS#10 en PEM.
 * Se usa para enviar a una CA y obtener un cert firmado.
 */
esp_err_t cert_get_csr(char *csr_out, size_t csr_buf_size);

/**
 * Carga un certificado firmado por una CA.
 * Valida que la clave pública del cert coincida con la del dispositivo.
 * Una vez cargado el dispositivo queda en estado PROVISIONED.
 */
esp_err_t cert_load_ca_signed(const char *cert_pem, size_t cert_len);

/**
 * Devuelve el estado actual del dispositivo.
 */
cert_state_t cert_get_state(void);

/**
 * Devuelve el fingerprint SHA-256 del certificado actual (hex, 64 chars + \0).
 */
esp_err_t cert_get_fingerprint(char *fingerprint_out);
