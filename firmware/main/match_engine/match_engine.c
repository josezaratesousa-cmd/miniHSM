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
