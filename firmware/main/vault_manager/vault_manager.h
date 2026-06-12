#pragma once

#include "esp_err.h"
#include "crypto_engine.h"

/**
 * @brief Inicializa el vault.
 *        Si no hay keypair guardado, genera uno y lo persiste en NVS.
 *        Si ya existe, lo carga.
 */
esp_err_t vault_init(void);

/**
 * @brief Firma un digest usando la clave privada del vault.
 *        La clave se carga en RAM solo durante esta operacion y se zeroiza al finalizar.
 *
 * @param digest        32 bytes (SHA-256)
 * @param sig_out       Buffer DER (min CRYPTO_SIG_DER_MAX_SIZE bytes)
 * @param sig_len_out   Longitud real de la firma
 */
esp_err_t vault_sign(
    const uint8_t *digest,
    uint8_t       *sig_out,
    size_t        *sig_len_out
);

/**
 * @brief Devuelve la clave publica del dispositivo.
 *        Se puede exponer libremente.
 *
 * @param pubkey_out    Buffer de CRYPTO_PUBKEY_SIZE bytes
 */
esp_err_t vault_get_pubkey(uint8_t *pubkey_out);

/**
 * @brief Devuelve el DeviceID (primeros 8 bytes de SHA-256 de la pubkey, hex).
 *        Buffer debe tener al menos 17 bytes.
 */
esp_err_t vault_get_device_id(char *device_id_out);

/**
 * @brief Borra el keypair del NVS (destruccion de clave).
 *        OPERACION IRREVERSIBLE.
 */
esp_err_t vault_destroy_key(void);

/* Solo para uso interno del cert_manager durante generacion de cert/CSR.
   La privkey se zeroiza inmediatamente despues de usarla. */
esp_err_t vault_get_privkey_raw(uint8_t *privkey_out, uint8_t *pubkey_out);

/* Fase 0 — secreto local del chip para la KEK de custodia.
   Opcion 1 (MVP): aleatorio de 32 bytes persistido en NVS. Encapsulado para migrar
   luego a eFuse/HMAC (Opcion 2) sin tocar el resto del codigo. */
esp_err_t vault_get_chip_kek_secret(uint8_t *secret_out);
