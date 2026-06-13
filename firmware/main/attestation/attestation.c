#include "attestation.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "esp_log.h"
#include "crypto_engine.h"
#include "vault_manager.h"
#include "custody_manager.h"
#include "cert_manager.h"
#include "version.h"

static const char *TAG = "attestation";

/* ===== CBOR writer (determinista, RFC 8949 4.2.1 por construccion) ===== */
typedef struct { uint8_t *buf; size_t cap; size_t len; bool ovf; } cbw_t;
static void cbw_byte(cbw_t *w, uint8_t b){ if (w->len < w->cap) w->buf[w->len]=b; else w->ovf=true; w->len++; }
static void cbw_raw(cbw_t *w, const uint8_t *p, size_t n){ for (size_t i=0;i<n;i++) cbw_byte(w,p[i]); }
static void cbw_head(cbw_t *w, int major, uint64_t arg){
    uint8_t mt=(uint8_t)(major<<5);
    if      (arg<24)    cbw_byte(w, mt|(uint8_t)arg);
    else if (arg<256){  cbw_byte(w, mt|24); cbw_byte(w,(uint8_t)arg); }
    else if (arg<65536){cbw_byte(w, mt|25); cbw_byte(w,(uint8_t)(arg>>8)); cbw_byte(w,(uint8_t)arg); }
    else {              cbw_byte(w, mt|26); cbw_byte(w,(uint8_t)(arg>>24)); cbw_byte(w,(uint8_t)(arg>>16));
                        cbw_byte(w,(uint8_t)(arg>>8)); cbw_byte(w,(uint8_t)arg); }
}
static void cbw_tstr(cbw_t *w, const char *s){ size_t n=strlen(s); cbw_head(w,3,n); cbw_raw(w,(const uint8_t*)s,n); }
static void cbw_bstr(cbw_t *w, const uint8_t *p, size_t n){ cbw_head(w,2,n); cbw_raw(w,p,n); }
static void cbw_arr(cbw_t *w, size_t n){ cbw_head(w,4,n); }
static void cbw_map(cbw_t *w, size_t n){ cbw_head(w,5,n); }

/* ===== base58btc ===== */
static int b58enc(const uint8_t *data, size_t len, char *out, size_t cap){
    static const char A[]="123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    uint8_t tmp[96];
    if (len>64) return -1;
    memset(tmp,0,sizeof(tmp));
    size_t zeros=0; while (zeros<len && data[zeros]==0) zeros++;
    for (size_t i=0;i<len;i++){
        uint32_t carry=data[i];
        for (size_t j=sizeof(tmp); j-- > 0; ){
            carry += (uint32_t)tmp[j]*256;
            tmp[j]=(uint8_t)(carry%58); carry/=58;
        }
    }
    size_t k=0; while (k<sizeof(tmp) && tmp[k]==0) k++;
    if (zeros + (sizeof(tmp)-k) + 1 > cap) return -1;
    size_t o=0;
    for (size_t i=0;i<zeros;i++) out[o++]='1';
    for (; k<sizeof(tmp); k++) out[o++]=A[tmp[k]];
    out[o]=0;
    return (int)o;
}

/* ===== did:key desde pubkey P-256 (04||X||Y, 65B) ===== */
static int att_did_key(const uint8_t *pub65, char *out, size_t cap){
    if (pub65[0]!=0x04) return -1;
    uint8_t data[35];
    data[0]=0x80; data[1]=0x24;                 /* multicodec p256-pub varint */
    data[2]=(pub65[64]&1)?0x03:0x02;            /* paridad de Y -> punto comprimido */
    memcpy(&data[3], &pub65[1], 32);            /* X */
    const char *pfx="did:key:z"; size_t pl=strlen(pfx);
    if (cap<pl+1) return -1;
    memcpy(out,pfx,pl);
    return (b58enc(data,35,out+pl,cap-pl)<0)?-1:0;
}

/* ===== payload de la VC (CBOR canonico) ===== */
typedef struct { const char *alias,*sigtype,*fpr,*state; } att_cred_t;
typedef struct {
    const char *issuer,*valid_from,*id,*model,*curve,*backend,*cert_state,*challenge;
    const att_cred_t *creds; size_t cred_count;
} att_vc_t;

static size_t att_payload(const att_vc_t *v, uint8_t *out, size_t cap){
    cbw_t w={out,cap,0,false};
    cbw_map(&w, v->challenge?6:5);
    cbw_tstr(&w,"type"); cbw_arr(&w,2); cbw_tstr(&w,"VerifiableCredential"); cbw_tstr(&w,"XamiDeviceCredential");
    cbw_tstr(&w,"issuer"); cbw_tstr(&w,v->issuer);
    cbw_tstr(&w,"@context"); cbw_arr(&w,1); cbw_tstr(&w,"https://www.w3.org/ns/credentials/v2");
    if (v->challenge){ cbw_tstr(&w,"challenge"); cbw_tstr(&w,v->challenge); }
    cbw_tstr(&w,"validFrom"); cbw_tstr(&w,v->valid_from);
    cbw_tstr(&w,"credentialSubject");
        cbw_map(&w,6);
        cbw_tstr(&w,"id");        cbw_tstr(&w,v->id);
        cbw_tstr(&w,"curve");     cbw_tstr(&w,v->curve);
        cbw_tstr(&w,"model");     cbw_tstr(&w,v->model);
        cbw_tstr(&w,"backend");   cbw_tstr(&w,v->backend);
        cbw_tstr(&w,"certState"); cbw_tstr(&w,v->cert_state);
        cbw_tstr(&w,"custodiedCredentials"); cbw_arr(&w,v->cred_count);
            for (size_t i=0;i<v->cred_count;i++){
                cbw_map(&w,4);
                cbw_tstr(&w,"alias");           cbw_tstr(&w,v->creds[i].alias);
                cbw_tstr(&w,"sigType");         cbw_tstr(&w,v->creds[i].sigtype);
                cbw_tstr(&w,"certState");       cbw_tstr(&w,v->creds[i].state);
                cbw_tstr(&w,"certFingerprint"); cbw_tstr(&w,v->creds[i].fpr);
            }
    return w.ovf?0:w.len;
}

/* Sig_structure = ["Signature1", h'a10126', h'', payload] */
static size_t att_sig_structure(const uint8_t *payload, size_t plen, uint8_t *out, size_t cap){
    static const uint8_t prot[3]={0xa1,0x01,0x26};
    cbw_t w={out,cap,0,false};
    cbw_arr(&w,4); cbw_tstr(&w,"Signature1"); cbw_bstr(&w,prot,3);
    cbw_bstr(&w,(const uint8_t*)"",0); cbw_bstr(&w,payload,plen);
    return w.ovf?0:w.len;
}

/* COSE_Sign1 = tag(18)[ h'a10126', {}, payload, sig ] */
static size_t att_cose_sign1(const uint8_t *payload, size_t plen, const uint8_t *sig, size_t siglen,
                             uint8_t *out, size_t cap){
    static const uint8_t prot[3]={0xa1,0x01,0x26};
    cbw_t w={out,cap,0,false};
    cbw_byte(&w,0xd2); cbw_arr(&w,4); cbw_bstr(&w,prot,3); cbw_map(&w,0);
    cbw_bstr(&w,payload,plen); cbw_bstr(&w,sig,siglen);
    return w.ovf?0:w.len;
}

/* ===== VC del device firmada (COSE_Sign1) ===== */
esp_err_t att_device_vc(const char *challenge,
                        char *cose_hex_out, size_t cose_hex_cap,
                        char *did_out, size_t did_cap)
{
    /* 1. pubkey del device -> did:key */
    uint8_t pub[CRYPTO_PUBKEY_SIZE];
    if (vault_get_pubkey(pub) != ESP_OK) return ESP_FAIL;
    char did[80];
    if (att_did_key(pub, did, sizeof(did)) != 0) return ESP_FAIL;
    if (did_out && did_cap) snprintf(did_out, did_cap, "%s", did);

    /* 2. validFrom (NTP) */
    char iso[32];
    time_t now = time(NULL); struct tm tmv; gmtime_r(&now, &tmv);
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &tmv);

    /* 3. credenciales custodiadas (sin passphrase) */
    static char aliases[CUSTODY_MAX_CREDS][CUSTODY_ALIAS_MAX];
    static char sigtypes[CUSTODY_MAX_CREDS][24];
    static char fprs[CUSTODY_MAX_CREDS][65];
    att_cred_t creds[CUSTODY_MAX_CREDS];
    size_t nc = 0;
    for (int slot = 0; slot < CUSTODY_MAX_CREDS; slot++) {
        if (custody_get_alias(slot, aliases[nc], sizeof(aliases[nc])) != ESP_OK) continue;
        int kind = CRYPTO_KEY_EC, bits = 256;
        custody_get_type(slot, &kind, &bits);
        crypto_sigtype_name(kind, bits, sigtypes[nc], sizeof(sigtypes[nc]));
        if (custody_get_fingerprint(slot, fprs[nc], sizeof(fprs[nc])) != ESP_OK) fprs[nc][0] = 0;
        creds[nc].alias = aliases[nc]; creds[nc].sigtype = sigtypes[nc];
        creds[nc].fpr = fprs[nc];      creds[nc].state = "PROVISIONED";
        nc++;
    }

    /* 4. payload CBOR */
    att_vc_t vc = {
        .issuer = did, .valid_from = iso, .id = did,
        .model = XAMI_MODEL, .curve = "P-256", .backend = "mbedTLS-PSA",
        .cert_state = (cert_get_state() == CERT_STATE_PROVISIONED) ? "PROVISIONED" : "UNPROVISIONED",
        .challenge = (challenge && challenge[0]) ? challenge : NULL,
        .creds = creds, .cred_count = nc
    };
    static uint8_t payload[1536];
    size_t plen = att_payload(&vc, payload, sizeof(payload));
    if (!plen) { ESP_LOGE(TAG, "payload overflow"); return ESP_ERR_NO_MEM; }

    /* 5. Sig_structure -> SHA-256 -> firma raw r||s del device */
    static uint8_t ss[1600];
    size_t sslen = att_sig_structure(payload, plen, ss, sizeof(ss));
    if (!sslen) { ESP_LOGE(TAG, "sig_structure overflow"); return ESP_ERR_NO_MEM; }
    uint8_t digest[CRYPTO_DIGEST_SIZE];
    if (crypto_sha256(ss, sslen, digest) != ESP_OK) return ESP_FAIL;
    uint8_t sig[CRYPTO_SIG_RAW_SIZE];
    if (vault_sign_raw(digest, sig) != ESP_OK) { ESP_LOGE(TAG, "vault_sign_raw fallo"); return ESP_FAIL; }

    /* 6. COSE_Sign1 -> hex */
    static uint8_t cose[1700];
    size_t clen = att_cose_sign1(payload, plen, sig, CRYPTO_SIG_RAW_SIZE, cose, sizeof(cose));
    if (!clen) { ESP_LOGE(TAG, "cose overflow"); return ESP_ERR_NO_MEM; }
    if (clen * 2 + 1 > cose_hex_cap) return ESP_ERR_NO_MEM;
    crypto_bytes_to_hex(cose, clen, cose_hex_out);
    return ESP_OK;
}
