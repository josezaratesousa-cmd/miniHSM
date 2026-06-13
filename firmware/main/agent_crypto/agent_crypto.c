#include "agent_crypto.h"
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "cc_helpers.h"
#include "vault_manager.h"
#include "crypto_engine.h"

static const char *TAG = "agent";

/* Ejercita el envelope del agente con el chip_kek_secret REAL del dispositivo:
 * R aleatorio + Kmaster + fingerprint -> K ; AES-256-GCM round-trip ; derivacion determinista. */
void agent_selftest(void)
{
    uint8_t chip[32], R[32], fp[32], K[32], K2[32];
    uint8_t priv[64], ct[64], tag[16], back[64], nonce[12];

    if (vault_get_chip_kek_secret(chip) != ESP_OK) {
        ESP_LOGW(TAG, "self-test: sin chip_kek_secret");
        return;
    }
    esp_fill_random(R, sizeof(R));
    esp_fill_random(nonce, sizeof(nonce));
    for (int i = 0; i < 32; i++) fp[i]   = (uint8_t)(i * 7 + 1);   /* fingerprint ficticio */
    for (int i = 0; i < 64; i++) priv[i] = (uint8_t)(255 - i);     /* privada de prueba    */

    if (cc_kek_derive(R, sizeof(R), chip, sizeof(chip), fp, sizeof(fp), K) != ESP_OK) {
        ESP_LOGE(TAG, "self-test: derive FAIL"); goto done;
    }
    if (cc_aead_encrypt(K, nonce, priv, sizeof(priv), ct, tag) != ESP_OK) {
        ESP_LOGE(TAG, "self-test: encrypt FAIL"); goto done;
    }
    if (cc_aead_decrypt(K, nonce, ct, sizeof(priv), tag, back) != ESP_OK) {
        ESP_LOGE(TAG, "self-test: decrypt/tag FAIL"); goto done;
    }
    if (memcmp(priv, back, sizeof(priv)) != 0) {
        ESP_LOGE(TAG, "self-test: roundtrip MISMATCH"); goto done;
    }
    /* mismo R+fp+Kmaster debe dar la misma K (re-derivacion en cada firma) */
    if (cc_kek_derive(R, sizeof(R), chip, sizeof(chip), fp, sizeof(fp), K2) != ESP_OK ||
        memcmp(K, K2, sizeof(K)) != 0) {
        ESP_LOGE(TAG, "self-test: derive no determinista"); goto done;
    }
    ESP_LOGI(TAG, "agente: self-test OK (envelope R+Kmaster+fingerprint -> AES-256-GCM)");
done:
    crypto_zeroize(chip, sizeof(chip)); crypto_zeroize(R, sizeof(R));
    crypto_zeroize(K, sizeof(K));       crypto_zeroize(K2, sizeof(K2));
    crypto_zeroize(priv, sizeof(priv)); crypto_zeroize(back, sizeof(back));
}
