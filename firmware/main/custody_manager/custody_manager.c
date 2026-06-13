#include "custody_manager.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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
#define TOTP_WRAP_SIZE (NONCE_SIZE + CUSTODY_TOTP_SEED_LEN + GCMTAG_SIZE)  /* 12+20+16=48 */
#define CHIP_SECRET_SIZE 32

typedef struct {
    char    alias[CUSTODY_ALIAS_MAX];
    uint8_t salt[SALT_SIZE];
    uint8_t fingerprint[32];
    uint8_t  in_use;
    uint8_t  sig_kind;   /* 0=EC, 1=RSA (crypto_key_kind_t) */
    uint16_t priv_len;   /* longitud del PKCS#8 DER (RSA ~1.2KB, EC ~138B) */
    uint16_t sig_bits;   /* 256, 2048, ... */
    uint8_t  mode;   /* 0=agente (sin TOTP), 1=autorizacion (TOTP por firma) */
    uint8_t  rsv[1];
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

esp_err_t custody_add(const char *alias, const uint8_t *priv_der, size_t priv_der_len, const char *cert_pem,
                      const uint8_t *passphrase, size_t pass_len,
                      uint8_t *totp_seed_out, size_t *totp_seed_len_out, int *slot_out, int mode){
    if (!alias || !priv_der || !cert_pem || !passphrase || !slot_out) return ESP_ERR_INVALID_ARG;
    if (priv_der_len == 0 || priv_der_len > CUSTODY_PRIV_DER_MAX){ ESP_LOGE(TAG, "FAIL priv_der_len=%u fuera de rango (max %d)", (unsigned)priv_der_len, CUSTODY_PRIV_DER_MAX); return ESP_ERR_INVALID_ARG; }
    size_t cert_len = strlen(cert_pem);
    if (cert_len == 0 || cert_len >= CUSTODY_CERT_MAX){ ESP_LOGE(TAG, "FAIL cert_len=%u fuera de rango (max %d)", (unsigned)cert_len, CUSTODY_CERT_MAX); return ESP_ERR_INVALID_ARG; }

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    int slot = find_free_slot(h);
    if (slot < 0){ nvs_close(h); return ESP_ERR_NO_MEM; }

    custody_meta_t m; memset(&m, 0, sizeof(m));
    strncpy(m.alias, alias, CUSTODY_ALIAS_MAX - 1);
    esp_fill_random(m.salt, SALT_SIZE);
    crypto_sha256((const uint8_t*)cert_pem, cert_len, m.fingerprint);
    m.in_use = 1;
    m.mode = (uint8_t)(mode ? 1 : 0);
    m.priv_len = (uint16_t)priv_der_len;
    {   int kind = CRYPTO_KEY_UNKNOWN, bits = 0;
        if (crypto_pk_type(priv_der, priv_der_len, &kind, &bits) != ESP_OK){ ESP_LOGE(TAG, "FAIL crypto_pk_type no parsea PKCS8 (priv_len=%u)", (unsigned)priv_der_len); nvs_close(h); return ESP_ERR_INVALID_ARG; }
        m.sig_kind = (uint8_t)kind; m.sig_bits = (uint16_t)bits;
        ESP_LOGI(TAG, "add: kind=%d bits=%d priv_len=%u cert_len=%u slot=%d", kind, bits, (unsigned)priv_der_len, (unsigned)cert_len, slot);
    }

    uint8_t chip[CHIP_SECRET_SIZE], kek[32];
    size_t  pwrap_len = NONCE_SIZE + priv_der_len + GCMTAG_SIZE;
    uint8_t *wrap = malloc(pwrap_len);          /* priv (PKCS#8) cifrada, longitud variable */
    if (!wrap){ nvs_close(h); return ESP_ERR_NO_MEM; }
    uint8_t seed[CUSTODY_TOTP_SEED_LEN];
    uint8_t twrap[TOTP_WRAP_SIZE];              /* semilla TOTP cifrada */
    esp_fill_random(seed, CUSTODY_TOTP_SEED_LEN);

    err = vault_get_chip_kek_secret(chip);
    if (err == ESP_OK)
        err = cc_kek_derive(passphrase, pass_len, chip, CHIP_SECRET_SIZE, m.salt, SALT_SIZE, kek);
    if (err == ESP_OK){
        esp_fill_random(wrap, NONCE_SIZE);
        err = cc_aead_encrypt(kek, wrap, priv_der, priv_der_len,
                             wrap + NONCE_SIZE, wrap + NONCE_SIZE + priv_der_len);
    }
    if (err == ESP_OK){
        esp_fill_random(twrap, NONCE_SIZE);
        err = cc_aead_encrypt(kek, twrap, seed, CUSTODY_TOTP_SEED_LEN,
                             twrap + NONCE_SIZE, twrap + NONCE_SIZE + CUSTODY_TOTP_SEED_LEN);
    }
    crypto_zeroize(kek, sizeof(kek));
    crypto_zeroize(chip, sizeof(chip));
    if (err != ESP_OK){ crypto_zeroize(seed,sizeof(seed)); free(wrap); nvs_close(h); return err; }

    char k[16]; uint64_t tc0 = 0;
    key_for(k, slot, "priv"); err  = nvs_set_blob(h, k, wrap, pwrap_len);
    key_for(k, slot, "totp"); if (err==ESP_OK) err = nvs_set_blob(h, k, twrap, TOTP_WRAP_SIZE);
    key_for(k, slot, "tc");   if (err==ESP_OK) err = nvs_set_u64(h, k, tc0);
    key_for(k, slot, "cert"); if (err==ESP_OK) err = nvs_set_str(h, k, cert_pem);
    key_for(k, slot, "meta"); if (err==ESP_OK) err = nvs_set_blob(h, k, &m, sizeof(m));
    if (err == ESP_OK) err = nvs_commit(h);
    if (err != ESP_OK) ESP_LOGE(TAG, "FAIL custody nvs err=0x%x (%s) slot=%d", err, esp_err_to_name(err), slot);
    crypto_zeroize(wrap, pwrap_len); free(wrap);
    crypto_zeroize(twrap, sizeof(twrap));
    nvs_close(h);
    if (err == ESP_OK){
        *slot_out = slot;
        if (totp_seed_out && totp_seed_len_out && *totp_seed_len_out >= CUSTODY_TOTP_SEED_LEN){
            memcpy(totp_seed_out, seed, CUSTODY_TOTP_SEED_LEN);
            *totp_seed_len_out = CUSTODY_TOTP_SEED_LEN;
        }
        ESP_LOGI(TAG, "credencial '%s' en slot %d (TOTP enrolado)", alias, slot);
    }
    crypto_zeroize(seed, sizeof(seed));
    return err;
}

esp_err_t custody_sign(int slot, const uint8_t *passphrase, size_t pass_len,
                       const char *totp_code, uint64_t unix_time,
                       const uint8_t *digest, uint8_t *sig_der_out, size_t *sig_der_len){
    if (slot < 0 || slot >= CUSTODY_MAX_CREDS || !passphrase || !totp_code || !digest ||
        !sig_der_out || !sig_der_len) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);   /* RW: actualiza anti-replay */
    if (err != ESP_OK) return err;

    custody_meta_t m;
    if (read_meta(h, slot, &m) != ESP_OK || !m.in_use){ nvs_close(h); return ESP_ERR_NOT_FOUND; }

    char k[16]; size_t len;
    size_t  pwrap_len = NONCE_SIZE + m.priv_len + GCMTAG_SIZE;
    uint8_t *pwrap = malloc(pwrap_len);
    uint8_t twrap[TOTP_WRAP_SIZE]; uint64_t last_tc = 0;
    if (!pwrap){ nvs_close(h); return ESP_ERR_NO_MEM; }
    key_for(k, slot, "priv"); len=pwrap_len; err = nvs_get_blob(h, k, pwrap, &len);
    if (err==ESP_OK && len!=pwrap_len) err=ESP_ERR_INVALID_SIZE;
    if (err==ESP_OK){ key_for(k, slot, "totp"); len=sizeof(twrap); err=nvs_get_blob(h,k,twrap,&len);
                      if (err==ESP_OK && len!=TOTP_WRAP_SIZE) err=ESP_ERR_INVALID_SIZE; }
    if (err==ESP_OK){ key_for(k, slot, "tc"); if (nvs_get_u64(h,k,&last_tc)!=ESP_OK) last_tc=0; }
    if (err != ESP_OK){ free(pwrap); nvs_close(h); return err; }

    uint8_t chip[CHIP_SECRET_SIZE], kek[32];
    uint8_t seed[CUSTODY_TOTP_SEED_LEN];
    uint8_t *priv_der = malloc(m.priv_len);
    uint64_t counter = 0;
    if (!priv_der){ free(pwrap); nvs_close(h); return ESP_ERR_NO_MEM; }
    err = vault_get_chip_kek_secret(chip);
    if (err==ESP_OK)
        err = cc_kek_derive(passphrase, pass_len, chip, CHIP_SECRET_SIZE, m.salt, SALT_SIZE, kek);
    /* descifrar semilla TOTP (passphrase mala -> tag invalido) */
    if (err==ESP_OK)
        err = cc_aead_decrypt(kek, twrap, twrap+NONCE_SIZE, CUSTODY_TOTP_SEED_LEN,
                              twrap+NONCE_SIZE+CUSTODY_TOTP_SEED_LEN, seed);
    /* GATE: validar TOTP + anti-replay de ventana */
    if (err==ESP_OK){
        esp_err_t v = cc_totp_verify(seed, CUSTODY_TOTP_SEED_LEN, unix_time, 30, 6, 1, totp_code, &counter);
        if (v != ESP_OK)            err = ESP_ERR_INVALID_STATE;   /* codigo invalido/expirado */
        else if (counter <= last_tc) err = ESP_ERR_INVALID_STATE;  /* ventana ya usada (replay) */
    }
    crypto_zeroize(seed, sizeof(seed));
    /* descifrar priv y firmar */
    if (err==ESP_OK)
        err = cc_aead_decrypt(kek, pwrap, pwrap+NONCE_SIZE, m.priv_len,
                              pwrap+NONCE_SIZE+m.priv_len, priv_der);
    crypto_zeroize(kek, sizeof(kek));
    crypto_zeroize(chip, sizeof(chip));
    crypto_zeroize(pwrap, pwrap_len); free(pwrap);
    crypto_zeroize(twrap, sizeof(twrap));
    if (err==ESP_OK) err = crypto_pk_sign(priv_der, m.priv_len, digest, sig_der_out, CRYPTO_PK_SIG_MAX, sig_der_len);
    crypto_zeroize(priv_der, m.priv_len); free(priv_der);

    if (err==ESP_OK){ key_for(k, slot, "tc"); nvs_set_u64(h, k, counter); nvs_commit(h); } /* anti-replay */
    nvs_close(h);
    return err;
}

esp_err_t custody_get_type(int slot, int *kind_out, int *bits_out){
    if (slot < 0 || slot >= CUSTODY_MAX_CREDS) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    custody_meta_t m;
    err = read_meta(h, slot, &m);
    nvs_close(h);
    if (err != ESP_OK || !m.in_use) return ESP_ERR_NOT_FOUND;
    if (kind_out) *kind_out = m.sig_kind;
    if (bits_out) *bits_out = m.sig_bits;
    return ESP_OK;
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

esp_err_t custody_get_fingerprint(int slot, char *hex_out, size_t cap){
    if (slot < 0 || slot >= CUSTODY_MAX_CREDS || cap < 65) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    custody_meta_t m;
    err = read_meta(h, slot, &m);
    nvs_close(h);
    if (err != ESP_OK || !m.in_use) return ESP_ERR_NOT_FOUND;
    crypto_bytes_to_hex(m.fingerprint, 32, hex_out);
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

esp_err_t custody_get_mode(int slot, int *mode_out){
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return ESP_FAIL;
    custody_meta_t m; esp_err_t e = read_meta(h, slot, &m); nvs_close(h);
    if (e != ESP_OK || !m.in_use) return ESP_FAIL;
    if (mode_out) *mode_out = m.mode;
    return ESP_OK;
}
