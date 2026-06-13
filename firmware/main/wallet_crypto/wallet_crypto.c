#include "wallet_crypto.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_random.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"

static const char *TAG = "wallet";

/* Vector T1 (mnemonica 'abandon...about', m/44'/60'/0'/0/0) */
static const char *VEC_PRIV = "1ab42cc412b618bdea3a599e3c9bae199ebf030895b039e9db1e30dafb12b727";
static const char *VEC_PUB  = "0237b0bb7a8288d38ed49a524b5dc98cff3eb5ca824c9f9dc0dfdb3d9cd600f299";

static int rng_cb(void *ctx, unsigned char *buf, size_t len) {
    (void)ctx; esp_fill_random(buf, len); return 0;
}

esp_err_t wallet_selftest(void) {
    mbedtls_ecp_group grp; mbedtls_ecp_point Q; mbedtls_mpi d;
    mbedtls_ecp_group_init(&grp); mbedtls_ecp_point_init(&Q); mbedtls_mpi_init(&d);
    esp_err_t out = ESP_FAIL;

    int ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1);
    if (ret) { ESP_LOGE(TAG, "secp256k1 NO disponible (ret=-0x%04x)", -ret); goto done; }

    if ((ret = mbedtls_mpi_read_string(&d, 16, VEC_PRIV)) != 0) goto done;
    if ((ret = mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, rng_cb, NULL)) != 0) {
        ESP_LOGE(TAG, "ecp_mul fallo (ret=-0x%04x)", -ret); goto done; }

    unsigned char buf[33]; size_t olen = 0;
    ret = mbedtls_ecp_point_write_binary(&grp, &Q, MBEDTLS_ECP_PF_COMPRESSED, &olen, buf, sizeof(buf));
    if (ret || olen != 33) { ESP_LOGE(TAG, "write_binary fallo (ret=%d olen=%u)", ret, (unsigned)olen); goto done; }

    char hex[67]; for (int i = 0; i < 33; i++) sprintf(hex + 2 * i, "%02x", buf[i]); hex[66] = 0;
    if (strcmp(hex, VEC_PUB) == 0) {
        ESP_LOGI(TAG, "secp256k1 self-test OK (pub=%s)", hex); out = ESP_OK;
    } else {
        ESP_LOGE(TAG, "secp256k1 self-test FAIL: got=%s exp=%s", hex, VEC_PUB);
    }
done:
    mbedtls_ecp_group_free(&grp); mbedtls_ecp_point_free(&Q); mbedtls_mpi_free(&d);
    return out;
}
