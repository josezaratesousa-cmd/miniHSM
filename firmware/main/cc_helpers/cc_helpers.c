#include "cc_helpers.h"
#include <string.h>
#include <stdlib.h>
#include "mbedtls/md.h"
#include "crypto_engine.h"   /* crypto_sha256 */
#include "mbedtls/hkdf.h"
#include "mbedtls/gcm.h"

/* ---------- base32 (RFC 4648) ---------- */
static int b32val(char c){
    if(c>='A'&&c<='Z') return c-'A';
    if(c>='a'&&c<='z') return c-'a';
    if(c>='2'&&c<='7') return c-'2'+26;
    return -1;
}

esp_err_t cc_base32_decode(const char *in, uint8_t *out, size_t *out_len){
    if(!in||!out||!out_len) return ESP_ERR_INVALID_ARG;
    size_t cap=*out_len, o=0; uint32_t buf=0; int bits=0;
    for(const char *p=in; *p; p++){
        if(*p=='='||*p==' '||*p=='\n'||*p=='\r'||*p=='\t') continue;
        int v=b32val(*p);
        if(v<0) return ESP_ERR_INVALID_ARG;
        buf=(buf<<5)|(uint32_t)v; bits+=5;
        if(bits>=8){ bits-=8;
            if(o>=cap) return ESP_ERR_NO_MEM;
            out[o++]=(uint8_t)((buf>>bits)&0xFF);
        }
    }
    *out_len=o; return ESP_OK;
}

static const char B32A[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

esp_err_t cc_base32_encode(const uint8_t *in, size_t in_len, char *out, size_t out_cap){
    if(!in||!out) return ESP_ERR_INVALID_ARG;
    size_t need=((in_len+4)/5)*8;
    if(out_cap < need+1) return ESP_ERR_NO_MEM;
    size_t o=0; uint32_t buf=0; int bits=0;
    for(size_t i=0;i<in_len;i++){
        buf=(buf<<8)|in[i]; bits+=8;
        while(bits>=5){ bits-=5; out[o++]=B32A[(buf>>bits)&0x1F]; }
    }
    if(bits>0) out[o++]=B32A[(buf<<(5-bits))&0x1F];
    while(o<need) out[o++]='=';
    out[o]=0; return ESP_OK;
}

/* ---------- TOTP (RFC 6238, HMAC-SHA1) ---------- */
static esp_err_t hotp_code(const uint8_t *key,size_t key_len,uint64_t counter,
                           int digits,char *out){
    uint8_t msg[8];
    for(int i=7;i>=0;i--){ msg[i]=(uint8_t)(counter&0xFF); counter>>=8; }
    uint8_t mac[20];
    const mbedtls_md_info_t *info=mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
    if(!info) return ESP_FAIL;
    if(mbedtls_md_hmac(info,key,key_len,msg,8,mac)!=0) return ESP_FAIL;
    int off=mac[19]&0x0F;
    uint32_t bin=((uint32_t)(mac[off]&0x7F)<<24)|((uint32_t)mac[off+1]<<16)|
                 ((uint32_t)mac[off+2]<<8)|((uint32_t)mac[off+3]);
    uint32_t mod=1; for(int i=0;i<digits;i++) mod*=10;
    uint32_t code=bin%mod;
    for(int i=digits-1;i>=0;i--){ out[i]=(char)('0'+(code%10)); code/=10; }
    out[digits]=0; return ESP_OK;
}

esp_err_t cc_totp(const uint8_t *key,size_t key_len,uint64_t unix_time,
                  uint32_t step,int digits,char *code_out){
    if(!key||!code_out||step==0||digits<1||digits>9) return ESP_ERR_INVALID_ARG;
    return hotp_code(key,key_len,unix_time/step,digits,code_out);
}

esp_err_t cc_totp_verify(const uint8_t *key,size_t key_len,uint64_t unix_time,
                         uint32_t step,int digits,int window,
                         const char *code,uint64_t *counter_out){
    if(!key||!code||step==0||digits<1||digits>9) return ESP_ERR_INVALID_ARG;
    if((int)strlen(code)!=digits) return ESP_ERR_INVALID_STATE;
    uint64_t c0=unix_time/step; char buf[10];
    for(int w=-window; w<=window; w++){
        uint64_t c=c0+(uint64_t)w;
        if(hotp_code(key,key_len,c,digits,buf)!=ESP_OK) return ESP_FAIL;
        int diff=0; for(int i=0;i<digits;i++) diff|=(buf[i]^code[i]);
        if(diff==0){ if(counter_out)*counter_out=c; return ESP_OK; }
    }
    return ESP_ERR_INVALID_STATE; /* codigo invalido/expirado */
}

/* ---------- Merkle root (SHA-256) ---------- */
esp_err_t cc_merkle_root(const uint8_t *leaves,size_t n,uint8_t *root_out){
    if(!root_out) return ESP_ERR_INVALID_ARG;
    if(n==0){ memset(root_out,0,32); return ESP_OK; }
    if(!leaves) return ESP_ERR_INVALID_ARG;
    uint8_t *cur=malloc((n+1)*32);   /* heap: no reventar el stack del task */
    if(!cur) return ESP_ERR_NO_MEM;
    memcpy(cur, leaves, n*32);
    size_t cnt=n; uint8_t pair[64];
    while(cnt>1){
        if(cnt&1){ memcpy(cur+cnt*32, cur+(cnt-1)*32, 32); cnt++; } /* duplica ultima */
        size_t half=cnt/2;
        for(size_t i=0;i<half;i++){
            memcpy(pair,      cur+(2*i)*32,   32);
            memcpy(pair+32,   cur+(2*i+1)*32, 32);
            if(crypto_sha256(pair,64,cur+i*32)!=ESP_OK){ free(cur); return ESP_FAIL; }
        }
        cnt=half;
    }
    memcpy(root_out, cur, 32);
    free(cur);
    return ESP_OK;
}

/* ---- Fase 1: KEK (HKDF) + AEAD (AES-256-GCM) ---- */
#define CC_KEK_INFO "xami-custody-kek-v1"

esp_err_t cc_kek_derive(const uint8_t *pass,size_t plen,const uint8_t *chip_secret,size_t cslen,
                        const uint8_t *salt,size_t saltlen,uint8_t *kek_out){
    if(!pass||!chip_secret||!kek_out) return ESP_ERR_INVALID_ARG;
    if(plen>256||cslen>64) return ESP_ERR_INVALID_ARG;
    uint8_t ikm[320];
    memcpy(ikm,pass,plen); memcpy(ikm+plen,chip_secret,cslen);
    const mbedtls_md_info_t *md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    int rc = md ? mbedtls_hkdf(md, salt, saltlen, ikm, plen+cslen,
                  (const unsigned char*)CC_KEK_INFO, strlen(CC_KEK_INFO), kek_out, 32)
                : -1;
    crypto_zeroize(ikm,sizeof(ikm));
    return rc==0?ESP_OK:ESP_FAIL;
}

esp_err_t cc_aead_encrypt(const uint8_t *kek,const uint8_t *nonce,const uint8_t *pt,size_t ptlen,
                          uint8_t *ct_out,uint8_t *tag_out){
    if(!kek||!nonce||!ct_out||!tag_out||(!pt&&ptlen)) return ESP_ERR_INVALID_ARG;
    mbedtls_gcm_context g; mbedtls_gcm_init(&g);
    int rc=mbedtls_gcm_setkey(&g,MBEDTLS_CIPHER_ID_AES,kek,256);
    if(rc==0) rc=mbedtls_gcm_crypt_and_tag(&g,MBEDTLS_GCM_ENCRYPT,ptlen,nonce,12,
                                           NULL,0,pt,ct_out,16,tag_out);
    mbedtls_gcm_free(&g);
    return rc==0?ESP_OK:ESP_FAIL;
}

esp_err_t cc_aead_decrypt(const uint8_t *kek,const uint8_t *nonce,const uint8_t *ct,size_t ctlen,
                          const uint8_t *tag,uint8_t *pt_out){
    if(!kek||!nonce||!tag||!pt_out||(!ct&&ctlen)) return ESP_ERR_INVALID_ARG;
    mbedtls_gcm_context g; mbedtls_gcm_init(&g);
    int rc=mbedtls_gcm_setkey(&g,MBEDTLS_CIPHER_ID_AES,kek,256);
    if(rc==0) rc=mbedtls_gcm_auth_decrypt(&g,ctlen,nonce,12,NULL,0,tag,16,ct,pt_out);
    mbedtls_gcm_free(&g);
    return rc==0?ESP_OK:ESP_ERR_INVALID_STATE;   /* tag invalido */
}
