#pragma once
/* Fase 4b — ceremonia de carga del .p12 (servida por la LAN del chip).
   El navegador abre el .p12 (forge), extrae cert+priv y manda un paquete ECIES
   {secret, alias, cert, priv, pass} cifrado hacia la pubkey del chip. El chip lo
   descifra, verifica el secreto de ceremonia (emitido por xami.run) y llama custody_add. */
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/* Arma la ceremonia pendiente (secreto esperado + alias), recibido del server por heartbeat. */
void ceremony_arm(const char *secret, const char *alias);

/* Indica si hay una ceremonia armada (para que el handler HTTP acepte el POST). */
int ceremony_is_armed(void);

/* Procesa el blob ECIES recibido: descifra -> verifica secreto -> custody_add ->
   arma la respuesta JSON con el slot y el otpauth:// (para el QR). */
esp_err_t ceremony_process(const uint8_t *blob, size_t blob_len, char *resp, size_t resp_cap);
