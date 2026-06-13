#include "network_engine.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_sntp.h"
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "cJSON.h"

#include "crypto_engine.h"
#include "vault_manager.h"
#include "cert_manager.h"
#include "policy_engine.h"
#include "audit_engine.h"
#include "version.h"
#include <time.h>
#include "wifi_provision.h"
#include "ceremony.h"
#include "attestation.h"
#include "esp_system.h"
#include "esp_random.h"

static const char *TAG = "network_engine";

#define WIFI_MAX_RETRY   5
#define FIRMWARE_VERSION XAMI_VERSION

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int              s_retry_num = 0;
static bool             s_connected_once = false;  /* true tras la 1a conexion exitosa */
static httpd_handle_t   s_server    = NULL;

/* ─────────────────────────────────────────────────────────────────────────── */
/*  WiFi                                                                        */
/* ─────────────────────────────────────────────────────────────────────────── */

static void wifi_event_handler(void *arg, esp_event_base_t eb,
                               int32_t id, void *data)
{
    if (eb == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (eb == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_connected_once) {
            /* Ya estuvimos conectados: el WiFi se cayo en operacion.
             * Reconectar INDEFINIDAMENTE (Opcion A). El router volvera
             * tarde o temprano y el mini se recupera solo, sin reiniciar. */
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            s_retry_num++;
            if (s_retry_num % 10 == 1) {
                ESP_LOGW(TAG, "WiFi perdido, reconectando indefinidamente (intento %d)...", s_retry_num);
            }
            /* Reconexion inmediata. Si no hay AP, el propio evento DISCONNECTED
             * vuelve a dispararse, creando un bucle de reintentos sin bloquear el
             * event loop. ESP-IDF espacia los intentos internamente. */
            esp_wifi_connect();
        } else if (s_retry_num < WIFI_MAX_RETRY) {
            /* Arranque inicial: reintentar unas veces */
            esp_wifi_connect();
            s_retry_num++;
        } else {
            /* Arranque fallido: marcar FAIL para disparar el portal de config */
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (eb == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        s_retry_num   = 0;
        s_connected_once = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t network_wifi_init(void)
{
    /* Cargar credenciales: NVS > Kconfig > provisioning mode */
    wifi_creds_t        creds  = {0};
    wifi_creds_source_t source = WIFI_SRC_NONE;

    if (wifi_provision_load(&creds, &source) != ESP_OK) {
        ESP_LOGW(TAG, "No WiFi credentials — skipping WiFi init");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Connecting to WiFi: %s (source=%s)",
             creds.ssid,
             source == WIFI_SRC_NVS    ? "NVS"    :
             source == WIFI_SRC_KCONFIG? "Kconfig" : "none");

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_wifi, inst_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &inst_wifi));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &inst_ip));

    wifi_config_t wifi_config = { .sta = { .threshold.authmode = WIFI_AUTH_WPA2_PSK } };
    strncpy((char *)wifi_config.sta.ssid,     creds.ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, creds.pass, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));

    return (bits & WIFI_CONNECTED_BIT) ? ESP_OK : ESP_FAIL;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  HTTP helpers                                                                */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t read_body(httpd_req_t *req, char *buf, size_t buf_sz)
{
    int total = req->content_len;
    if (total <= 0 || total >= (int)buf_sz) return ESP_ERR_INVALID_SIZE;
    int recv = 0;
    while (recv < total) {
        int r = httpd_req_recv(req, buf + recv, total - recv);
        if (r <= 0) return ESP_FAIL;
        recv += r;
    }
    buf[recv] = '\0';
    return ESP_OK;
}

static void send_json(httpd_req_t *req, int code, const char *body)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (code != 200) {
        httpd_resp_set_status(req,
            code == 400 ? "400 Bad Request"   :
            code == 401 ? "401 Unauthorized"  :
            code == 409 ? "409 Conflict"      :
                          "500 Internal Server Error");
    }
    httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static void send_err(httpd_req_t *req, int code,
                     const char *err_code, const char *msg)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"errorCode\":\"%s\",\"message\":\"%s\"}", err_code, msg);
    send_json(req, code, buf);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  POST /sign                                                                  */
/*  Devuelve firma DER + certificado PEM (listo para PAdES/CAdES/XAdES)        */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_sign(httpd_req_t *req)
{
    char body[512];
    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        send_err(req, 400, "ERR001", "Invalid body");
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) { send_err(req, 400, "ERR002", "JSON parse error"); return ESP_OK; }

    const char *req_id     = cJSON_GetStringValue(cJSON_GetObjectItem(root, "requestId"));
    const char *digest_hex = cJSON_GetStringValue(cJSON_GetObjectItem(root, "digest"));
    const char *kuser      = cJSON_GetStringValue(cJSON_GetObjectItem(root, "kuser"));
    cJSON      *ts_item    = cJSON_GetObjectItem(root, "timestamp");
    cJSON      *nc_item    = cJSON_GetObjectItem(root, "nonce");

    if (!digest_hex || strlen(digest_hex) != 64) {
        cJSON_Delete(root);
        send_err(req, 400, "ERR003", "digest: 64 hex chars (SHA-256)");
        audit_log(AUDIT_OP_SIGN, req_id ? req_id : "?", digest_hex, AUDIT_RESULT_FAIL);
        return ESP_OK;
    }

    if (!kuser || !ts_item || !nc_item) {
        cJSON_Delete(root);
        send_err(req, 401, "ERR004", "Missing auth fields (kuser/timestamp/nonce)");
        audit_log(AUDIT_OP_SIGN, req_id ? req_id : "?", digest_hex, AUDIT_RESULT_DENIED);
        return ESP_OK;
    }

    int64_t     ts    = (int64_t)cJSON_GetNumberValue(ts_item);
    const char *nonce = cJSON_GetStringValue(nc_item);

    if (policy_validate_token(kuser, ts, nonce) != ESP_OK) {
        cJSON_Delete(root);
        send_err(req, 401, "ERR005", "Invalid or expired token");
        audit_log(AUDIT_OP_SIGN, req_id ? req_id : "?", digest_hex, AUDIT_RESULT_DENIED);
        return ESP_OK;
    }

    uint8_t digest[CRYPTO_DIGEST_SIZE];
    if (crypto_hex_to_bytes(digest_hex, digest, CRYPTO_DIGEST_SIZE) != ESP_OK) {
        cJSON_Delete(root);
        send_err(req, 400, "ERR006", "Invalid digest hex");
        audit_log(AUDIT_OP_SIGN, req_id ? req_id : "?", digest_hex, AUDIT_RESULT_FAIL);
        return ESP_OK;
    }

    /* Firma en DER */
    uint8_t sig_der[CRYPTO_SIG_DER_MAX_SIZE];
    size_t  sig_der_len = 0;
    if (vault_sign(digest, sig_der, &sig_der_len) != ESP_OK) {
        cJSON_Delete(root);
        send_err(req, 500, "ERR007", "Signing operation failed");
        audit_log(AUDIT_OP_SIGN, req_id ? req_id : "?", digest_hex, AUDIT_RESULT_FAIL);
        return ESP_OK;
    }

    char sig_hex[CRYPTO_SIG_DER_MAX_SIZE * 2 + 1];
    crypto_bytes_to_hex(sig_der, sig_der_len, sig_hex);

    /* Certificado */
    char cert_pem[CERT_PEM_MAX_SIZE];
    cert_get_pem(cert_pem, sizeof(cert_pem));

    char device_id[17];
    vault_get_device_id(device_id);

    /* Construir respuesta — cert como campo JSON (escapar newlines) */
    /* Usamos cJSON para manejar el PEM correctamente */
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "requestId",   req_id ? req_id : "unknown");
    cJSON_AddStringToObject(resp, "status",      "success");
    cJSON_AddStringToObject(resp, "signature",   sig_hex);
    cJSON_AddStringToObject(resp, "algorithm",   "ECDSA-P256-SHA256-DER");
    cJSON_AddStringToObject(resp, "certificate", cert_pem);
    cJSON_AddStringToObject(resp, "deviceId",    device_id);
    cJSON_AddStringToObject(resp, "certState",
        cert_get_state() == CERT_STATE_PROVISIONED ? "PROVISIONED" : "UNPROVISIONED");

    char *resp_str = cJSON_PrintUnformatted(resp);
    send_json(req, 200, resp_str);
    free(resp_str);
    cJSON_Delete(resp);

    audit_log(AUDIT_OP_SIGN, req_id ? req_id : "?", digest_hex, AUDIT_RESULT_OK);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  POST /verify                                                                */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_verify(httpd_req_t *req)
{
    char body[1024];
    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        send_err(req, 400, "ERR001", "Invalid body"); return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) { send_err(req, 400, "ERR002", "JSON parse error"); return ESP_OK; }

    const char *req_id     = cJSON_GetStringValue(cJSON_GetObjectItem(root, "requestId"));
    const char *digest_hex = cJSON_GetStringValue(cJSON_GetObjectItem(root, "digest"));
    const char *sig_hex    = cJSON_GetStringValue(cJSON_GetObjectItem(root, "signature"));

    if (!digest_hex || !sig_hex) {
        cJSON_Delete(root);
        send_err(req, 400, "ERR003", "Missing digest or signature");
        return ESP_OK;
    }

    uint8_t digest[CRYPTO_DIGEST_SIZE];
    if (crypto_hex_to_bytes(digest_hex, digest, CRYPTO_DIGEST_SIZE) != ESP_OK) {
        cJSON_Delete(root); send_err(req, 400, "ERR006", "Invalid digest"); return ESP_OK;
    }

    size_t  sig_len = strlen(sig_hex) / 2;
    uint8_t sig[CRYPTO_SIG_DER_MAX_SIZE];
    if (sig_len > CRYPTO_SIG_DER_MAX_SIZE) {
        cJSON_Delete(root); send_err(req, 400, "ERR008", "Signature too long"); return ESP_OK;
    }
    for (size_t i = 0; i < sig_len; i++) {
        unsigned int b; sscanf(sig_hex + i * 2, "%02x", &b); sig[i] = (uint8_t)b;
    }

    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];
    vault_get_pubkey(pubkey);

    int valid = 0;
    crypto_verify(digest, sig, sig_len, pubkey, &valid);

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"requestId\":\"%s\",\"status\":\"success\",\"valid\":%s}",
        req_id ? req_id : "unknown", valid ? "true" : "false");

    send_json(req, 200, resp);
    audit_log(AUDIT_OP_VERIFY, req_id ? req_id : "?", digest_hex,
              valid ? AUDIT_RESULT_OK : AUDIT_RESULT_FAIL);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* ─────────────────────────────────────────────────────────────────────────── */
/*  Bloque 0 — envelope comun de atestacion (/device /health /audit)            */
/* ─────────────────────────────────────────────────────────────────────────── */

/* "Xami-<MODELO>-<DeviceID>" */
static void att_device_name(char *out, size_t cap)
{
    char id[17]; vault_get_device_id(id);
    snprintf(out, cap, "Xami-%s-%s", XAMI_MODEL, id);
}

/* firmware { version, build, release }  (release = hash corto del commit) */
static cJSON *att_firmware_obj(void)
{
    cJSON *fw = cJSON_CreateObject();
    cJSON_AddStringToObject(fw, "version", XAMI_VERSION);
    cJSON_AddStringToObject(fw, "build",   XAMI_BUILD_NUMBER);
    cJSON_AddStringToObject(fw, "release", XAMI_GIT_COMMIT);
    return fw;
}

/* Agrega "time" (ISO 8601 UTC, o null si el reloj no es confiable) + "timeSynced".
   Toda entrega de informacion externa lleva su sello de tiempo (base del Bloque 7). */
static void att_add_time(cJSON *obj)
{
    bool synced = network_time_synced();
    if (synced) {
        time_t now = time(NULL);
        struct tm tmv; gmtime_r(&now, &tmv);
        char iso[32];
        strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &tmv);
        cJSON_AddStringToObject(obj, "time", iso);
    } else {
        cJSON_AddNullToObject(obj, "time");
    }
    cJSON_AddBoolToObject(obj, "timeSynced", synced);
}

/*  GET /device/challenge — nonce de un solo uso para frescura de la VC          */
/* ─────────────────────────────────────────────────────────────────────────── */
static char    s_dev_challenge[33] = {0};
static bool    s_dev_chal_used = true;
static int64_t s_dev_chal_ts = 0;
#define DEV_CHAL_TTL_S 120

static esp_err_t handler_device_challenge(httpd_req_t *req)
{
    uint8_t rnd[16]; esp_fill_random(rnd, sizeof(rnd));
    crypto_bytes_to_hex(rnd, sizeof(rnd), s_dev_challenge);
    s_dev_chal_used = false;
    s_dev_chal_ts   = esp_timer_get_time() / 1000000LL;

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "challenge", s_dev_challenge);
    cJSON_AddNumberToObject(resp, "ttl", DEV_CHAL_TTL_S);
    att_add_time(resp);
    char *s = cJSON_PrintUnformatted(resp);
    send_json(req, 200, s);
    free(s); cJSON_Delete(resp);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  GET /device                                                                 */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_device(httpd_req_t *req)
{
    /* challenge opcional (?challenge=...) para frescura de la VC */
    const char *use_chal = NULL;
    char chal[40] = {0};
    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen > 0 && qlen < 96) {
        char qbuf[96];
        if (httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf)) == ESP_OK)
            httpd_query_key_value(qbuf, "challenge", chal, sizeof(chal));
    }
    if (chal[0]) {
        int64_t age = esp_timer_get_time() / 1000000LL - s_dev_chal_ts;
        if (!s_dev_chal_used && strcmp(chal, s_dev_challenge) == 0 && age <= DEV_CHAL_TTL_S) {
            s_dev_chal_used = true;
            use_chal = chal;
        } else {
            send_err(req, 400, "ERR_CHAL", "invalid or expired challenge");
            return ESP_OK;
        }
    }

    char name[48]; att_device_name(name, sizeof(name));

    uint8_t pubkey[CRYPTO_PUBKEY_SIZE];
    vault_get_pubkey(pubkey);
    char pubkey_hex[CRYPTO_PUBKEY_SIZE * 2 + 1];
    crypto_bytes_to_hex(pubkey, CRYPTO_PUBKEY_SIZE, pubkey_hex);

    char fingerprint[65];
    cert_get_fingerprint(fingerprint);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "deviceId",    name);
    cJSON_AddItemToObject(resp,   "firmware",    att_firmware_obj());
    cJSON_AddStringToObject(resp, "pubkey",      pubkey_hex);
    cJSON_AddStringToObject(resp, "backend",     "mbedTLS-PSA");
    cJSON_AddStringToObject(resp, "curve",       "P-256");
    cJSON_AddStringToObject(resp, "sigFormat",   "DER");
    cJSON_AddStringToObject(resp, "certFingerprint", fingerprint);
    cJSON_AddStringToObject(resp, "certState",
        cert_get_state() == CERT_STATE_PROVISIONED ? "PROVISIONED" : "UNPROVISIONED");
    att_add_time(resp);

    /* proof: Verifiable Credential del device como COSE_Sign1 (ES256), en hex */
    char did[80] = {0};
    static char cose_hex[3600];
    if (att_device_vc(use_chal, cose_hex, sizeof(cose_hex), did, sizeof(did)) == ESP_OK) {
        cJSON *proof = cJSON_CreateObject();
        cJSON_AddStringToObject(proof, "type", "CoseSign1ES256");
        cJSON_AddStringToObject(proof, "did",  did);
        cJSON_AddStringToObject(proof, "cose", cose_hex);
        cJSON_AddItemToObject(resp, "proof", proof);
    }

    char *s = cJSON_PrintUnformatted(resp);
    send_json(req, 200, s);
    free(s);
    cJSON_Delete(resp);
    audit_log(AUDIT_OP_DEVICE, "system", NULL, AUDIT_RESULT_OK);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  GET /health                                                                 */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_health(httpd_req_t *req)
{
    int64_t  uptime = esp_timer_get_time() / 1000000LL;
    uint32_t ops    = audit_get_count();
    char name[48]; att_device_name(name, sizeof(name));

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddStringToObject(resp, "device", name);
    cJSON_AddStringToObject(resp, "model",  XAMI_MODEL);
    cJSON_AddItemToObject(resp, "firmware", att_firmware_obj());
    cJSON_AddStringToObject(resp, "cert",
        cert_get_state() == CERT_STATE_PROVISIONED ? "ca-signed" : "self-signed");
    cJSON_AddNumberToObject(resp, "uptime",  (double)uptime);
    cJSON_AddNumberToObject(resp, "opCount", (double)ops);
    att_add_time(resp);
    cJSON_AddBoolToObject(resp, "secureBoot", false);

    char *s = cJSON_PrintUnformatted(resp);
    send_json(req, 200, s);
    free(s);
    cJSON_Delete(resp);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  GET /cert  — devuelve el certificado PEM actual                            */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_cert_get(httpd_req_t *req)
{
    char pem[CERT_PEM_MAX_SIZE];
    if (cert_get_pem(pem, sizeof(pem)) != ESP_OK) {
        send_err(req, 500, "ERR010", "No certificate available");
        return ESP_OK;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "certificate", pem);
    cJSON_AddStringToObject(resp, "state",
        cert_get_state() == CERT_STATE_PROVISIONED ? "PROVISIONED" : "UNPROVISIONED");

    char *s = cJSON_PrintUnformatted(resp);
    send_json(req, 200, s);
    free(s);
    cJSON_Delete(resp);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  GET /csr  — devuelve CSR PKCS#10 para enviar a una CA                     */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_csr(httpd_req_t *req)
{
    char csr[CERT_CSR_MAX_SIZE];
    if (cert_get_csr(csr, sizeof(csr)) != ESP_OK) {
        send_err(req, 500, "ERR011", "CSR generation failed");
        return ESP_OK;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "csr", csr);
    cJSON_AddStringToObject(resp, "note",
        "Submit this CSR to your CA to obtain a signed certificate");

    char *s = cJSON_PrintUnformatted(resp);
    send_json(req, 200, s);
    free(s);
    cJSON_Delete(resp);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  POST /cert  — carga certificado firmado por una CA                         */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_cert_load(httpd_req_t *req)
{
    char body[CERT_PEM_MAX_SIZE + 256];
    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        send_err(req, 400, "ERR001", "Invalid body"); return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) { send_err(req, 400, "ERR002", "JSON parse error"); return ESP_OK; }

    const char *cert_pem = cJSON_GetStringValue(cJSON_GetObjectItem(root, "certificate"));
    if (!cert_pem) {
        cJSON_Delete(root);
        send_err(req, 400, "ERR012", "Missing 'certificate' field (PEM)");
        return ESP_OK;
    }

    esp_err_t err = cert_load_ca_signed(cert_pem, strlen(cert_pem));
    cJSON_Delete(root);

    if (err != ESP_OK) {
        send_err(req, 400, "ERR013", "Certificate rejected — pubkey mismatch");
        return ESP_OK;
    }

    char resp[128];
    snprintf(resp, sizeof(resp),
        "{\"status\":\"ok\",\"state\":\"PROVISIONED\","
        "\"message\":\"CA certificate loaded successfully\"}");
    send_json(req, 200, resp);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  GET /audit                                                                  */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_audit(httpd_req_t *req)
{
    char *buf = malloc(4096);
    if (!buf) { send_err(req, 500, "ERR010", "OOM"); return ESP_OK; }
    audit_get_json(buf, 4096);
    send_json(req, 200, buf);
    free(buf);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_provision_wifi(httpd_req_t *req);
static esp_err_t handler_provision_wifi_clear(httpd_req_t *req);
static esp_err_t handler_provision_reconfigure(httpd_req_t *req);

/* Fase 4b: cargador minimo de la UI de custodia. Sirve una pagina http (mismo origen que
   el chip) que arranca app.js desde xami.run. Asi la pagina puede hablar con el chip (http)
   y con la API (https) sin chocar con mixed-content. */
static esp_err_t handler_custodia(httpd_req_t *req)
{
    static const char *page =
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Custodia Xami</title></head><body>"
        "<script src=\"https://xami.run/custodia/app.js\"></script>"
        "</body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, page);
    return ESP_OK;
}

/* Fase 4b: ceremonia de custodia. Recibe el blob ECIES (binario) del navegador en la
   LAN, lo procesa (descifra -> verifica secreto -> custody_add) y responde el otpauth. */
static esp_err_t handler_ceremony(httpd_req_t *req)
{
    char resp[320];
    int total = req->content_len;
    if (total <= 0 || total > 8192) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"bad length\"}");
        return ESP_OK;
    }
    uint8_t *body = malloc(total);
    if (!body) { httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"no mem\"}"); return ESP_OK; }
    int received = 0, r;
    while (received < total) {
        r = httpd_req_recv(req, (char *)body + received, total - received);
        if (r <= 0) { free(body); httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"recv\"}"); return ESP_OK; }
        received += r;
    }
    ceremony_process(body, total, resp, sizeof(resp));
    memset(body, 0, total);
    free(body);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

esp_err_t network_http_server_start(void)
{
    httpd_config_t config  = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.stack_size       = 8192;

    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed");
        return ESP_FAIL;
    }

    const httpd_uri_t uris[] = {
        { .uri = "/sign",   .method = HTTP_POST, .handler = handler_sign      },
        { .uri = "/verify", .method = HTTP_POST, .handler = handler_verify    },
        { .uri = "/cert",   .method = HTTP_GET,  .handler = handler_cert_get  },
        { .uri = "/cert",   .method = HTTP_POST, .handler = handler_cert_load },
        { .uri = "/csr",    .method = HTTP_GET,  .handler = handler_csr       },
        { .uri = "/device/challenge", .method = HTTP_GET, .handler = handler_device_challenge },
        { .uri = "/device", .method = HTTP_GET,  .handler = handler_device    },
        { .uri = "/health", .method = HTTP_GET,  .handler = handler_health    },
        { .uri = "/audit",  .method = HTTP_GET,  .handler = handler_audit     },
        { .uri = "/provision/wifi", .method = HTTP_POST,   .handler = handler_provision_wifi       },
        { .uri = "/provision/wifi", .method = HTTP_DELETE, .handler = handler_provision_wifi_clear },
        { .uri = "/provision/reconfigure", .method = HTTP_POST, .handler = handler_provision_reconfigure },
        { .uri = "/custodia", .method = HTTP_GET,  .handler = handler_custodia },
        { .uri = "/ceremony", .method = HTTP_POST, .handler = handler_ceremony },
    };

    for (size_t i = 0; i < sizeof(uris)/sizeof(uris[0]); i++)
        httpd_register_uri_handler(s_server, &uris[i]);

    ESP_LOGI(TAG, "HTTP server started");
    ESP_LOGI(TAG, "POST: /sign /verify /cert /ceremony /provision/{wifi,reconfigure}");
    ESP_LOGI(TAG, "GET : /cert /csr /device /device/challenge /health /audit /custodia");
    return ESP_OK;
}

esp_err_t network_http_server_stop(void)
{
    if (s_server) { httpd_stop(s_server); s_server = NULL; }
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  POST /provision/wifi                                                        */
/*  Guarda credenciales WiFi en NVS. Requiere token valido.                    */
/*  El dispositivo reinicia para conectarse con la nueva red.                   */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_provision_wifi(httpd_req_t *req)
{
    char body[512];
    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        send_err(req, 400, "ERR001", "Invalid body"); return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) { send_err(req, 400, "ERR002", "JSON parse error"); return ESP_OK; }

    const char *kuser    = cJSON_GetStringValue(cJSON_GetObjectItem(root, "kuser"));
    const char *ssid     = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ssid"));
    const char *pass     = cJSON_GetStringValue(cJSON_GetObjectItem(root, "pass"));
    cJSON      *ts_item  = cJSON_GetObjectItem(root, "timestamp");
    cJSON      *nc_item  = cJSON_GetObjectItem(root, "nonce");

    if (!ssid || strlen(ssid) == 0) {
        cJSON_Delete(root);
        send_err(req, 400, "ERR003", "ssid required");
        return ESP_OK;
    }

    if (!kuser || !ts_item || !nc_item) {
        cJSON_Delete(root);
        send_err(req, 401, "ERR004", "Missing auth fields");
        return ESP_OK;
    }

    int64_t ts    = (int64_t)cJSON_GetNumberValue(ts_item);
    const char *nonce = cJSON_GetStringValue(nc_item);

    if (policy_validate_token(kuser, ts, nonce) != ESP_OK) {
        cJSON_Delete(root);
        send_err(req, 401, "ERR005", "Invalid or expired token");
        return ESP_OK;
    }

    esp_err_t err = wifi_provision_save(ssid, pass ? pass : "");
    cJSON_Delete(root);

    if (err != ESP_OK) {
        send_err(req, 500, "ERR009", "Failed to save credentials");
        return ESP_OK;
    }

    send_json(req, 200,
        "{\"status\":\"ok\",\"message\":\"WiFi credentials saved. Restarting...\"}");

    /* Reiniciar en 1 segundo para conectarse con la nueva red */
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  DELETE /provision/wifi — borra credenciales WiFi del NVS                   */
/* ─────────────────────────────────────────────────────────────────────────── */

static esp_err_t handler_provision_wifi_clear(httpd_req_t *req)
{
    wifi_provision_clear();
    send_json(req, 200,
        "{\"status\":\"ok\",\"message\":\"WiFi credentials cleared from NVS\"}");
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*  POST /provision/reconfigure                                                 */
/*  Reconfiguracion remota desde el serverHSM (con red activa).                 */
/*  Requiere KUser valido. Borra credenciales y reinicia al portal Xami.        */
/*  Queda registrado en auditoria quien lo solicito.                            */
/* ─────────────────────────────────────────────────────────────────────────── */
static esp_err_t handler_provision_reconfigure(httpd_req_t *req)
{
    char body[256];
    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        send_err(req, 400, "ERR001", "Invalid body");
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) { send_err(req, 400, "ERR002", "JSON parse error"); return ESP_OK; }

    const char *kuser   = cJSON_GetStringValue(cJSON_GetObjectItem(root, "kuser"));
    const char *req_id  = cJSON_GetStringValue(cJSON_GetObjectItem(root, "requestId"));
    cJSON      *ts_item = cJSON_GetObjectItem(root, "timestamp");
    cJSON      *nc_item = cJSON_GetObjectItem(root, "nonce");

    if (!kuser || !ts_item || !nc_item) {
        cJSON_Delete(root);
        audit_log(AUDIT_OP_RECONFIG, req_id ? req_id : "?", NULL, AUDIT_RESULT_DENIED);
        send_err(req, 401, "ERR004", "Missing auth fields");
        return ESP_OK;
    }

    int64_t ts        = (int64_t)cJSON_GetNumberValue(ts_item);
    const char *nonce = cJSON_GetStringValue(nc_item);

    if (policy_validate_token(kuser, ts, nonce) != ESP_OK) {
        cJSON_Delete(root);
        audit_log(AUDIT_OP_RECONFIG, req_id ? req_id : "?", NULL, AUDIT_RESULT_DENIED);
        send_err(req, 401, "ERR005", "Invalid or expired token");
        return ESP_OK;
    }

    /* Token valido: registrar y proceder */
    audit_log(AUDIT_OP_RECONFIG, req_id ? req_id : "?", NULL, AUDIT_RESULT_OK);
    cJSON_Delete(root);

    ESP_LOGW(TAG, "Reconfiguracion remota solicitada — entrando al portal Xami en 3s");
    send_json(req, 200,
        "{\"status\":\"ok\",\"message\":\"Entrando en modo configuracion Xami en 3 segundos\"}");

    /* Borrar credenciales y reiniciar: al no haber WiFi, main.c levanta el portal */
    vTaskDelay(pdMS_TO_TICKS(3000));
    wifi_provision_clear();
    esp_restart();
    return ESP_OK;
}

/* ---- Fase 0: NTP / hora (prerequisito de TOTP) ---- */
static volatile bool s_time_synced = false;

static void sntp_synced_cb(struct timeval *tv)
{
    (void)tv;
    s_time_synced = true;
    ESP_LOGI(TAG, "Hora sincronizada por NTP");
}

esp_err_t network_sntp_start(void)
{
    if (esp_sntp_enabled()) return ESP_OK;
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(sntp_synced_cb);
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP iniciado (pool.ntp.org)");
    return ESP_OK;
}

bool network_time_synced(void)
{
    return s_time_synced;
}
