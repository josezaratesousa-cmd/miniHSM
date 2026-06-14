#pragma once
/* Custodia LOCAL del agente automatizado (complementa custody_manager).
   Guarda, por slot de credencial:
     - R (32B) pendiente de ENTREGA al server (estado transitorio: desde la
       ceremonia hasta el ACK del heartbeat; luego se olvida).
     - nonce monotono ANTI-REPLAY (el server lo envia en cada firma agente;
       el chip exige nonce estrictamente creciente por credencial).
   NVS namespace propio ("agentst"); no toca el namespace de custody. */
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t agent_store_init(void);

/* Retiene R(32B)+fingerprint(32B) pendiente de entrega para el slot.
   Resetea el nonce del slot a 0 (es una credencial nueva). */
esp_err_t agent_pending_set(int slot, const uint8_t fp[32], const uint8_t r[32]);
/* Lee el pendiente del slot: ESP_OK si hay, ESP_ERR_NVS_NOT_FOUND si no. */
esp_err_t agent_pending_get(int slot, uint8_t fp[32], uint8_t r[32]);
/* Olvida R del slot (tras el ACK del server). El nonce se conserva. */
esp_err_t agent_pending_clear(int slot);
/* True si el slot tiene R pendiente de entrega. */
bool      agent_pending_has(int slot);

/* Anti-replay: true si nonce > ultimo_visto del slot (no persiste todavia). */
bool      agent_nonce_ok(int slot, uint64_t nonce);
/* Persiste nonce como ultimo_visto del slot (llamar tras firma OK). */
esp_err_t agent_nonce_commit(int slot, uint64_t nonce);
