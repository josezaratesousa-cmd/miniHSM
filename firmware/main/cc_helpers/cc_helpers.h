#pragma once
/* Fase 0 — helpers de custodia: base32 (RFC4648), TOTP (RFC6238), Merkle (SHA-256).
   Logica validada contra vectores conocidos (ver docs/PLAN_CUSTODIA_FASES.md, Fase 0). */
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/* base32 RFC4648. decode tolera '=', espacios y minusculas.
   En decode: *out_len = capacidad de entrada -> longitud real a la salida. */
esp_err_t cc_base32_decode(const char *in, uint8_t *out, size_t *out_len);
esp_err_t cc_base32_encode(const uint8_t *in, size_t in_len, char *out, size_t out_cap);

/* TOTP RFC6238 (HMAC-SHA1). code_out debe tener digits+1 bytes. */
esp_err_t cc_totp(const uint8_t *key, size_t key_len, uint64_t unix_time,
                  uint32_t step, int digits, char *code_out);

/* Valida 'code' con ventana +/- window pasos (tolerancia de reloj).
   Si valido: ESP_OK y *counter_out = contador de la ventana aceptada (para anti-replay). */
esp_err_t cc_totp_verify(const uint8_t *key, size_t key_len, uint64_t unix_time,
                         uint32_t step, int digits, int window,
                         const char *code, uint64_t *counter_out);

/* Merkle root. leaves = n hojas contiguas de 32 bytes (cada hoja = SHA256(doc)).
   interno = SHA256(L||R); nivel impar duplica la ultima hoja. root_out = 32 bytes. */
esp_err_t cc_merkle_root(const uint8_t *leaves, size_t n, uint8_t *root_out);
