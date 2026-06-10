#include "match_engine.h"
#include "vault_manager.h"
#include "crypto_engine.h"
#include "esp_log.h"
#include <string.h>

#include "mbedtls/ecdh.h"
#include "mbedtls/ecp.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md.h"

static const char *TAG = "match";

#define ECIES_EPHPUB_LEN  65
#define ECIES_IV_LEN      12
#define ECIES_TAG_LEN     16
#define ECIES_MIN_LEN     (ECIES_EPHPUB_LEN + ECIES_IV_LEN + ECIES_TAG_LEN)

static const uint8_t HKDF_INFO[] = "xami-match-v1";

esp_err_t match_ecies_decrypt(const uint8_t *blob, size_t blob_len,
                              uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (blob_len < ECIES_MIN_LEN) {
        ESP_LOGE(TAG, "blob demasiado corto (%d)", (int)blob_len);
        return ESP_ERR_INVALID_SIZE;
    }

    const uint8_t *eph_pub = blob;
    const uint8_t *iv      = blob + ECIES_EPHPUB_LEN;
    const uint8_t *ct      = blob + ECIES_EPHPUB_LEN + ECIES_IV_LEN;
    size_t ct_len_with_tag = blob_len - ECIES_EPHPUB_LEN - ECIES_IV_LEN;
    if (ct_len_with_tag < ECIES_TAG_LEN) return ESP_ERR_INVALID_SIZE;
    size_t ct_len = ct_len_with_tag - ECIES_TAG_LEN;
    const uint8_t *tag = ct + ct_len;

    if (ct_len > out_cap) {
        ESP_LOGE(TAG, "out buffer chico");
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t ret = ESP_FAIL;
    int rc;

    /* Cargar la privada del device */
    uint8_t privkey[CRYPTO_PRIVKEY_SIZE];
    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];
    if (vault_get_privkey_raw(privkey, pubkey) != ESP_OK) {
        ESP_LOGE(TAG, "no se pudo cargar privkey");
        return ESP_FAIL;
    }

    mbedtls_ecp_group grp;
    mbedtls_ecp_point Q_eph;        /* pubkey efimera del server */
    mbedtls_mpi d;                  /* privada del device */
    mbedtls_mpi z;                  /* shared secret X coord */
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&Q_eph);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&z);

    uint8_t shared[32];
    uint8_t aeskey[32];

    do {
        rc = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
        if (rc) { ESP_LOGE(TAG, "group_load %d", rc); break; }

        /* Cargar eph_pub (65 bytes uncompressed) */
        rc = mbedtls_ecp_point_read_binary(&grp, &Q_eph, eph_pub, ECIES_EPHPUB_LEN);
        if (rc) { ESP_LOGE(TAG, "read eph_pub %d", rc); break; }

        /* Cargar privada del device */
        rc = mbedtls_mpi_read_binary(&d, privkey, CRYPTO_PRIVKEY_SIZE);
        if (rc) { ESP_LOGE(TAG, "read priv %d", rc); break; }

        /* ECDH: z = (d * Q_eph).X */
        rc = mbedtls_ecdh_compute_shared(&grp, &z, &Q_eph, &d, NULL, NULL);
        if (rc) { ESP_LOGE(TAG, "ecdh %d", rc); break; }

        rc = mbedtls_mpi_write_binary(&z, shared, sizeof(shared));
        if (rc) { ESP_LOGE(TAG, "write shared %d", rc); break; }

        /* HKDF-SHA256(salt=NULL, info="xami-match-v1") -> 32 bytes */
        const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        uint8_t salt32[32] = {0};  /* salt explicito de 32 ceros (coincide con server) */
        rc = mbedtls_hkdf(md, salt32, sizeof(salt32), shared, sizeof(shared),
                          HKDF_INFO, sizeof(HKDF_INFO) - 1,  /* sin el \0 */
                          aeskey, sizeof(aeskey));
        if (rc) { ESP_LOGE(TAG, "hkdf %d", rc); break; }

        /* AES-256-GCM decrypt */
        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);
        rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, aeskey, 256);
        if (rc == 0) {
            rc = mbedtls_gcm_auth_decrypt(&gcm, ct_len,
                                          iv, ECIES_IV_LEN,
                                          NULL, 0,
                                          tag, ECIES_TAG_LEN,
                                          ct, out);
        }
        mbedtls_gcm_free(&gcm);
        if (rc) { ESP_LOGE(TAG, "gcm decrypt %d (tag invalido?)", rc); break; }

        *out_len = ct_len;
        ret = ESP_OK;
    } while (0);

    /* limpiar secretos */
    crypto_zeroize(privkey, sizeof(privkey));
    crypto_zeroize(shared, sizeof(shared));
    crypto_zeroize(aeskey, sizeof(aeskey));
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&Q_eph);
    mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&z);

    return ret;
}


/* ─────────────────────────────────────────────────────────────────────────── */
/* match_perform — emparejamiento con el server (Bloque 9)                      */
/* ─────────────────────────────────────────────────────────────────────────── */

#include "policy_engine.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "esp_timer.h"

#define MATCH_MAX_URL   256
#define MATCH_MAX_RESP  2048

esp_err_t match_perform(const char *server_url)
{
    if (policy_has_secret()) {
        ESP_LOGI(TAG, "Device ya emparejado (tiene secret), match no necesario");
        return ESP_OK;
    }

    char device_id[32];
    if (vault_get_device_id(device_id) != ESP_OK) return ESP_FAIL;

    /* pubkey en hex (65 bytes -> 130 hex) */
    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];
    if (vault_get_pubkey(pubkey) != ESP_OK) return ESP_FAIL;
    char pubkey_hex[CRYPTO_PUBKEY_SIZE * 2 + 1];
    crypto_bytes_to_hex(pubkey, CRYPTO_PUBKEY_SIZE, pubkey_hex);

    /* challenge = "deviceId:ts:nonce" */
    int64_t ts = esp_timer_get_time() / 1000000LL;
    char nonce[17];
    snprintf(nonce, sizeof(nonce), "%lld", (long long)(ts ^ 0x9A1C));
    char challenge[96];
    int ch_len = snprintf(challenge, sizeof(challenge), "%s:%lld:%s",
                          device_id, (long long)ts, nonce);

    /* firma: SHA256(challenge) -> vault_sign -> DER */
    uint8_t digest[CRYPTO_DIGEST_SIZE];
    if (crypto_sha256((const uint8_t *)challenge, ch_len, digest) != ESP_OK)
        return ESP_FAIL;
    uint8_t sig_der[CRYPTO_SIG_DER_MAX_SIZE];
    size_t sig_len = 0;
    if (vault_sign(digest, sig_der, &sig_len) != ESP_OK) return ESP_FAIL;
    char sig_hex[CRYPTO_SIG_DER_MAX_SIZE * 2 + 1];
    crypto_bytes_to_hex(sig_der, sig_len, sig_hex);

    /* body JSON */
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "deviceId",  device_id);
    cJSON_AddStringToObject(body, "pubkey",    pubkey_hex);
    cJSON_AddNumberToObject(body, "timestamp", (double)ts);
    cJSON_AddStringToObject(body, "nonce",     nonce);
    cJSON_AddStringToObject(body, "signature", sig_hex);
    cJSON_AddStringToObject(body, "model",     "A1");
    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    char url[MATCH_MAX_URL];
    snprintf(url, sizeof(url), "%.*s/devices/match",
             (int)(MATCH_MAX_URL - 20), server_url);

    char resp_buf[MATCH_MAX_RESP] = {0};
    esp_http_client_config_t cfg = {
        .url               = url,
        .method            = HTTP_METHOD_POST,
        .timeout_ms        = 10000,
        .transport_type    = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body_str, strlen(body_str));

    esp_err_t ret = ESP_FAIL;
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        esp_http_client_read_response(client, resp_buf, MATCH_MAX_RESP - 1);
        if (status == 200) {
            cJSON *resp = cJSON_Parse(resp_buf);
            cJSON *enc = resp ? cJSON_GetObjectItem(resp, "secretsEncrypted") : NULL;
            if (enc && cJSON_IsString(enc)) {
                /* hex -> bytes */
                size_t blob_len = strlen(enc->valuestring) / 2;
                uint8_t *blob = malloc(blob_len);
                if (blob && crypto_hex_to_bytes(enc->valuestring, blob, blob_len) == ESP_OK) {
                    uint8_t secret[32];
                    size_t out_len = 0;
                    if (match_ecies_decrypt(blob, blob_len, secret, sizeof(secret), &out_len) == ESP_OK
                        && out_len == 32) {
                        if (policy_set_secret(secret) == ESP_OK) {
                            ESP_LOGI(TAG, "MATCH OK: secret recibido y guardado");
                            ret = ESP_OK;
                        }
                    } else {
                        ESP_LOGE(TAG, "fallo descifrado ECIES del secret");
                    }
                    crypto_zeroize(secret, sizeof(secret));
                }
                free(blob);
            }
            if (resp) cJSON_Delete(resp);
        } else {
            ESP_LOGE(TAG, "match HTTP %d: %s", status, resp_buf);
        }
    } else {
        ESP_LOGE(TAG, "match request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    free(body_str);
    return ret;
}
