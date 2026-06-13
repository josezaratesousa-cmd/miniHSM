#include "ceremony.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_log.h"
#include "cJSON.h"
#include "match_engine.h"      /* match_ecies_decrypt */
#include "custody_manager.h"   /* custody_add, CUSTODY_* */
#include "cc_helpers.h"        /* cc_totp_uri */
#include "crypto_engine.h"     /* crypto_hex_to_bytes, crypto_zeroize, CRYPTO_PRIVKEY_SIZE */

static const char *TAG = "ceremony";
static char s_secret[40] = {0};
static char s_alias[CUSTODY_ALIAS_MAX] = {0};
static bool s_armed = false;

void ceremony_arm(const char *secret, const char *alias){
    strncpy(s_secret, secret ? secret : "", sizeof(s_secret) - 1); s_secret[sizeof(s_secret)-1] = 0;
    strncpy(s_alias,  alias  ? alias  : "", sizeof(s_alias)  - 1); s_alias[sizeof(s_alias)-1]   = 0;
    s_armed = (s_secret[0] != 0);
    ESP_LOGI(TAG, "ceremonia armada (alias='%s')", s_alias);
}

int ceremony_is_armed(void){ return s_armed ? 1 : 0; }

esp_err_t ceremony_process(const uint8_t *blob, size_t blob_len, char *resp, size_t resp_cap){
    if (!blob || !resp) return ESP_ERR_INVALID_ARG;
    if (!s_armed){ snprintf(resp, resp_cap, "{\"ok\":false,\"error\":\"no ceremony armed\"}");
                   return ESP_ERR_INVALID_STATE; }

    uint8_t *plain = malloc(2560);    /* cert PEM ~1-2KB + priv + pass */
    if (!plain){ snprintf(resp, resp_cap, "{\"ok\":false,\"error\":\"no mem\"}"); return ESP_ERR_NO_MEM; }
    size_t plain_len = 0;
    esp_err_t err = match_ecies_decrypt(blob, blob_len, plain, 2559, &plain_len);
    if (err != ESP_OK){ free(plain); snprintf(resp, resp_cap, "{\"ok\":false,\"error\":\"decrypt failed\"}");
                        return err; }
    plain[plain_len] = 0;

    cJSON *j = cJSON_Parse((char*)plain);
    const char *secret = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "secret")) : NULL;
    const char *alias  = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "alias"))  : NULL;
    const char *cert   = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "cert"))   : NULL;
    const char *privh  = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "priv"))   : NULL;
    const char *pass   = j ? cJSON_GetStringValue(cJSON_GetObjectItem(j, "pass"))   : NULL;

    esp_err_t rc = ESP_OK;
    if (!secret || !cert || !privh || !pass){
        snprintf(resp, resp_cap, "{\"ok\":false,\"error\":\"missing fields\"}"); rc = ESP_ERR_INVALID_ARG;
    } else if (strcmp(secret, s_secret) != 0){
        snprintf(resp, resp_cap, "{\"ok\":false,\"error\":\"bad ceremony secret\"}"); rc = ESP_ERR_INVALID_STATE;
    } else if (strlen(privh) < 64 || strlen(privh) > CUSTODY_PRIV_DER_MAX * 2 || (strlen(privh) & 1)){
        snprintf(resp, resp_cap, "{\"ok\":false,\"error\":\"bad priv\"}"); rc = ESP_ERR_INVALID_ARG;
    }

    if (rc == ESP_OK){
        if (!alias || !alias[0]) alias = s_alias;
        size_t priv_len = strlen(privh) / 2;
        uint8_t *priv_der = malloc(priv_len);
        if (!priv_der || crypto_hex_to_bytes(privh, priv_der, priv_len) != ESP_OK){
            free(priv_der);
            snprintf(resp, resp_cap, "{\"ok\":false,\"error\":\"bad priv hex\"}"); rc = ESP_ERR_INVALID_ARG;
        } else {
            uint8_t seed[CUSTODY_TOTP_SEED_LEN]; size_t seed_len = sizeof(seed); int slot = -1;
            rc = custody_add(alias, priv_der, priv_len, cert, (const uint8_t*)pass, strlen(pass),
                             seed, &seed_len, &slot);
            crypto_zeroize(priv_der, priv_len); free(priv_der);
            if (rc != ESP_OK){
                snprintf(resp, resp_cap, "{\"ok\":false,\"error\":\"custody_add failed\"}");
            } else {
                char label[80], uri[256];
                snprintf(label, sizeof(label), "Xami:%s", alias);
                if (cc_totp_uri(seed, seed_len, label, "Xami", uri, sizeof(uri)) == ESP_OK)
                    snprintf(resp, resp_cap, "{\"ok\":true,\"slot\":%d,\"otpauth\":\"%s\"}", slot, uri);
                else
                    snprintf(resp, resp_cap, "{\"ok\":true,\"slot\":%d}", slot);
                s_armed = false; s_secret[0] = 0;     /* secreto de un solo uso: consumido */
            }
            crypto_zeroize(seed, sizeof(seed));
        }
    }

    crypto_zeroize(plain, 2560); free(plain);
    if (j) cJSON_Delete(j);
    return rc;
}
