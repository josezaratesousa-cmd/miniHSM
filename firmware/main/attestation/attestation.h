#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/* Construye la Verifiable Credential del device (W3C 2.0) asegurada con COSE_Sign1 / ES256.
   El payload (CBOR canonico) lleva issuer=did:key, validFrom (NTP), credentialSubject con
   custodiedCredentials [{alias,sigType,certFingerprint,certState}], y el proof es la firma
   del device (raw r||s) sobre el Sig_structure. Salida: COSE_Sign1 en hex.
   challenge: nonce de /device/challenge (frescura) o NULL. did_out recibe el did:key. */
esp_err_t att_device_vc(const char *challenge,
                        char *cose_hex_out, size_t cose_hex_cap,
                        char *did_out, size_t did_cap);
