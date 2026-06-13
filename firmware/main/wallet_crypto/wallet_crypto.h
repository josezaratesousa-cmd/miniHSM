#pragma once
#include "esp_err.h"

/* Self-test de la cadena cripto EVM (T1): deriva la pubkey secp256k1 de una privkey
   conocida y la compara con el vector estandar. Valida que la curva esta operativa. */
esp_err_t wallet_selftest(void);
