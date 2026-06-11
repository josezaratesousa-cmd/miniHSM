#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/*
 * Match Engine — Bloque 9
 * Descifrado ECIES de los secretos que el server envia en el emparejamiento.
 *
 * Esquema (debe coincidir con el server, crypto_match.py):
 *   ECDH(privada_device, eph_pubkey) -> HKDF-SHA256(info="xami-match-v1") -> AES-256-GCM
 *   blob = eph_pubkey(65) || iv(12) || ciphertext || tag(16)
 */

/*
 * Descifra un blob ECIES usando la privada del device.
 * blob:      bytes del blob (eph_pub||iv||ct||tag)
 * blob_len:  longitud total del blob
 * out:       buffer de salida para el plaintext
 * out_cap:   capacidad del buffer out
 * out_len:   (salida) longitud del plaintext descifrado
 */
esp_err_t match_ecies_decrypt(const uint8_t *blob, size_t blob_len,
                              uint8_t *out, size_t out_cap, size_t *out_len);

/* Empareja el device con el server: firma challenge, recibe secret cifrado,
   lo descifra y lo guarda. No-op si ya tiene secret. */
esp_err_t match_perform(const char *server_url);

/* Lanza el match en su propia task con stack grande (no en main). */
void match_start(const char *server_url);
