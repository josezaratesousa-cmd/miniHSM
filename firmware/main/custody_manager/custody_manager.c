#include "custody_manager.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"
#include "cc_helpers.h"
#include "vault_manager.h"   /* vault_get_chip_kek_secret */

static const char *TAG    = "custody";
static const char *NVS_NS = "custody";

#define SALT_SIZE   16
#define NONCE_SIZE  12
#define GCMTAG_SIZE 16
#define WRAP_SIZE   (NONCE_SIZE + CRYPTO_PRIVKEY_SIZE + GCMTAG_SIZE)  /* 12+32+16=60 */
#define CHIP_SECRET_SIZE 32

typedef struct {
    char    alias[CUSTODY_ALIAS_MAX];
    uint8_t salt[SALT_SIZE];
    uint8_t fingerprint[32];
    uint8_t in_use;
    uint8_t rsv[3];
} custody_meta_t;

static void key_for(char *out, int slot, const char *suf){
    snprintf(out, 16, "c%d_%s", slot, suf);   /* p.ej. c0_priv, c12_meta (<=15) */
}

static esp_err_t read_meta(nvs_handle_t h, int slot, custody_meta_t *m){
    char k[16]; key_for(k, slot, "meta");
    size_t len = sizeof(*m);
    return nvs_get_blob(h, k, m, &len);
}

esp_err_t custody_init(void){
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);   /* crea el namespace si no existe */
    if (err == ESP_OK) nvs_close(h);
    ESP_LOGI(TAG, "custody_manager init (max %d credenciales)", CUSTODY_MAX_CREDS);
    return err;
}

int custody_count(void){
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return 0;
    int n = 0; custody_meta_t m;
    for (int i = 0; i < CUSTODY_MAX_CREDS; i++)
        if (read_meta(h, i, &m) == ESP_OK && m.in_use) n++;
    nvs_close(h);
    return n;
}

static int find_free_slot(nvs_handle_t h){
    custody_meta_t m;
    for (int i = 0; i < CUSTODY_MAX_CREDS; i++){
        esp_err_t e = read_meta(h, i, &m);
        if (e != ESP_OK || !m.in_use) return i;
    }
    return -1;
}

esp_err_t custody_add(const char *alias, const uint8_t *priv, const char *cert_pem,
                      const uint8_t *passphrase, size_t pass_len, int *slot_out){
    if (!alias || !priv || !cert_pem || !passphrase || !slot_out) return ESP_ERR_INVALID_ARG;
    size_t cert_len = strlen(cert_pem);
    if (cert_len == 0 || cert_len >= CUSTODY_CERT_MAX) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    int slot = find_free_slot(h);
    if (slot < 0){ nvs_close(h); return ESP_ERR_NO_MEM; }   /* sin slots libres */

    custody_meta_t m; memset(&m, 0, sizeof(m));
    strncpy(m.alias, alias, CUSTODY_ALIAS_MAX - 1);
    esp_fill_random(m.salt, SALT_SIZE);
    crypto_sha256((const uint8_t*)cert_pem, cert_len, m.fingerprint);
    m.in_use = 1;

    uint8_t chip[CHIP_SECRET_SIZE], kek[32], wrap[WRAP_SIZE];
    err = vault_get_chip_kek_secret(chip);
    if (err == ESP_OK)
        err = cc_kek_derive(passphrase, pass_len, chip, CHIP_SECRET_SIZE,
                            m.salt, SALT_SIZE, kek);
    if (err == ESP_OK){
        esp_fill_random(wrap, NONCE_SIZE);                                  /* nonce */
        err = cc_aead_encrypt(kek, wrap, priv, CRYPTO_PRIVKEY_SIZE,
                             wrap + NONCE_SIZE, wrap + NONCE_SIZE + CRYPTO_PRIVKEY_SIZE);
    }
    crypto_zeroize(kek, sizeof(kek));
    crypto_zeroize(chip, sizeof(chip));
    if (err != ESP_OK){ nvs_close(h); return err; }

    char k[16];
    key_for(k, slot, "priv"); err  = nvs_set_blob(h, k, wrap, WRAP_SIZE);
    key_for(k, slot, "cert"); if (err==ESP_OK) err = nvs_set_str(h, k, cert_pem);
    key_for(k, slot, "meta"); if (err==ESP_OK) err = nvs_set_blob(h, k, &m, sizeof(m));
    if (err == ESP_OK) err = nvs_commit(h);
    crypto_zeroize(wrap, sizeof(wrap));
    nvs_close(h);
    if (err == ESP_OK){ *slot_out = slot; ESP_LOGI(TAG, "credencial '%s' en slot %d", alias, slot); }
    return err;
}

esp_err_t custody_sign(int slot, const uint8_t *passphrase, size_t pass_len,
                       const uint8_t *digest, uint8_t *sig_der_out, size_t *sig_der_len){
    if (slot < 0 || slot >= CUSTODY_MAX_CREDS || !passphrase || !digest ||
        !sig_der_out || !sig_der_len) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    custody_meta_t m;
    if (read_meta(h, slot, &m) != ESP_OK || !m.in_use){ nvs_close(h); return ESP_ERR_NOT_FOUND; }

    char k[16]; key_for(k, slot, "priv");
    uint8_t wrap[WRAP_SIZE]; size_t wl = sizeof(wrap);
    err = nvs_get_blob(h, k, wrap, &wl);
    nvs_close(h);
    if (err != ESP_OK || wl != WRAP_SIZE) return ESP_ERR_NOT_FOUND;

    uint8_t chip[CHIP_SECRET_SIZE], kek[32], priv[CRYPTO_PRIVKEY_SIZE];
    err = vault_get_chip_kek_secret(chip);
    if (err == ESP_OK)
        err = cc_kek_derive(passphrase, pass_len, chip, CHIP_SECRET_SIZE,
                            m.salt, SALT_SIZE, kek);
    if (err == ESP_OK)
        err = cc_aead_decrypt(kek, wrap, wrap + NONCE_SIZE, CRYPTO_PRIVKEY_SIZE,
                             wrap + NONCE_SIZE + CRYPTO_PRIVKEY_SIZE, priv);  /* INVALID_STATE si passphrase mala */
    crypto_zeroize(kek, sizeof(kek));
    crypto_zeroize(chip, sizeof(chip));
    crypto_zeroize(wrap, sizeof(wrap));

    if (err == ESP_OK) err = crypto_sign(digest, priv, sig_der_out, sig_der_len);
    crypto_zeroize(priv, sizeof(priv));
    return err;
}

esp_err_t custody_get_cert(int slot, char *pem_out, size_t pem_cap){
    if (slot < 0 || slot >= CUSTODY_MAX_CREDS || !pem_out) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    char k[16]; key_for(k, slot, "cert");
    size_t len = pem_cap;
    err = nvs_get_str(h, k, pem_out, &len);
    nvs_close(h);
    return err;
}

esp_err_t custody_get_alias(int slot, char *alias_out, size_t cap){
    if (slot < 0 || slot >= CUSTODY_MAX_CREDS || !alias_out || cap == 0) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    custody_meta_t m;
    err = read_meta(h, slot, &m);
    nvs_close(h);
    if (err != ESP_OK || !m.in_use) return ESP_ERR_NOT_FOUND;
    strncpy(alias_out, m.alias, cap - 1);
    alias_out[cap - 1] = 0;
    return ESP_OK;
}

esp_err_t custody_delete(int slot){
    if (slot < 0 || slot >= CUSTODY_MAX_CREDS) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    char k[16];
    key_for(k, slot, "priv"); nvs_erase_key(h, k);
    key_for(k, slot, "cert"); nvs_erase_key(h, k);
    key_for(k, slot, "meta"); nvs_erase_key(h, k);
    err = nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "credencial del slot %d eliminada", slot);
    return err;
}
