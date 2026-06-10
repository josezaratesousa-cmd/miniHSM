#include "captive_portal.h"
#include "wifi_provision.h"
#include "vault_manager.h"
#include "version.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include <stdlib.h>
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "cJSON.h"

static const char *TAG = "captive_portal";

static httpd_handle_t   s_portal_httpd = NULL;
static TaskHandle_t     s_dns_task     = NULL;
static volatile bool    s_running      = false;
static volatile int64_t s_last_activity = 0;
static const char *PORTAL_HTML =
"<!DOCTYPE html><html lang=\"es\"><head>\n"
"<meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
"<title>Xami - Configuracion</title><style>\n"
"*{margin:0;padding:0;box-sizing:border-box;font-family:-apple-system,Segoe UI,Roboto,sans-serif}\n"
"body{background:linear-gradient(135deg,#0a2540 0%,#1a4a7a 100%);min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}\n"
".card{background:#fff;border-radius:20px;box-shadow:0 20px 60px rgba(0,0,0,.3);max-width:420px;width:100%;overflow:hidden}\n"
".hdr{background:linear-gradient(135deg,#0a2540,#1a4a7a);color:#fff;padding:36px 28px;text-align:center}\n"
".logo{font-size:34px;font-weight:700;letter-spacing:2px;margin-bottom:6px}\n"
".logo span{color:#4db8ff}\n"
".tag{font-size:14px;opacity:.85}\n"
".body{padding:28px}\n"
".welcome{font-size:15px;color:#444;line-height:1.6;margin-bottom:22px;text-align:center}\n"
"label{display:block;font-size:13px;font-weight:600;color:#0a2540;margin:14px 0 6px}\n"
"select,input{width:100%;padding:13px 14px;border:1.5px solid #d0d7de;border-radius:10px;font-size:15px;transition:border .2s}\n"
"select:focus,input:focus{outline:none;border-color:#1a4a7a}\n"
".btn{width:100%;margin-top:22px;padding:15px;background:linear-gradient(135deg,#0a2540,#1a4a7a);color:#fff;border:none;border-radius:10px;font-size:16px;font-weight:600;cursor:pointer;transition:opacity .2s}\n"
".btn:active{opacity:.85}\n"
".btn:disabled{opacity:.5;cursor:not-allowed}\n"
".scan{font-size:13px;color:#1a4a7a;text-align:center;margin-top:10px;cursor:pointer;text-decoration:underline}\n"
".ftr{text-align:center;padding:16px;font-size:11px;color:#9aa5b1;border-top:1px solid #eef1f4}\n"
".msg{padding:12px;border-radius:8px;font-size:14px;margin-top:14px;text-align:center;display:none}\n"
".msg.ok{background:#e6f7ed;color:#1a7f43;display:block}\n"
".msg.err{background:#fdeaea;color:#c0392b;display:block}\n"
".sig{color:#4db8ff;font-weight:600}\n"
"</style></head><body>\n"
"<div class=\"card\">\n"
"<div class=\"hdr\"><div class=\"logo\">X<span>a</span>mi</div><div class=\"tag\">Modulo de Firma Segura</div></div>\n"
"<div class=\"body\">\n"
"<p class=\"welcome\">Bienvenido. Conecta tu Xami a una red WiFi para comenzar a firmar documentos de forma segura.</p>\n"
"<label>Red WiFi</label>\n"
"<select id=\"ssid\"><option>Buscando redes...</option></select>\n"
"<div class=\"scan\" onclick=\"scan()\">Volver a buscar redes</div>\n"
"<label>Contrasena</label>\n"
"<input type=\"password\" id=\"pass\" placeholder=\"Contrasena de la red\">\n"
"<button class=\"btn\" id=\"go\" onclick=\"save()\">Conectar</button>\n"
"<div class=\"msg\" id=\"msg\"></div>\n"
"</div>\n"
"<div class=\"ftr\">__VERSION__</div>\n"
"</div>\n"
"<script>\n"
"function scan(){var s=document.getElementById('ssid');s.innerHTML='<option>Buscando...</option>';\n"
"fetch('/scan').then(r=>r.json()).then(d=>{s.innerHTML='';\n"
"if(!d.networks||!d.networks.length){s.innerHTML='<option>No se encontraron redes</option>';return}\n"
"d.networks.forEach(n=>{var o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+' dBm)';s.appendChild(o)})})\n"
".catch(e=>{s.innerHTML='<option>Error al buscar</option>'})}\n"
"function save(){var ssid=document.getElementById('ssid').value,pass=document.getElementById('pass').value,m=document.getElementById('msg'),b=document.getElementById('go');\n"
"if(!ssid){m.className='msg err';m.textContent='Selecciona una red';return}\n"
"b.disabled=true;b.textContent='Conectando...';m.className='msg';m.style.display='none';\n"
"fetch('/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid,pass:pass})})\n"
".then(r=>r.json()).then(d=>{if(d.status==='ok'){m.className='msg ok';m.textContent='Guardado. Xami se reiniciara y conectara a tu red.';}else{m.className='msg err';m.textContent=d.message||'Error';b.disabled=false;b.textContent='Conectar';}})\n"
".catch(e=>{m.className='msg ok';m.textContent='Guardado. Xami se esta reiniciando...';})}\n"
"window.onload=scan;\n"
"</script></body></html>";


/* ---- helpers ---- */
static void touch_activity(void) {
    s_last_activity = esp_timer_get_time() / 1000000;
}

/* GET / y cualquier ruta -> sirve el portal (captive).
 * Reemplaza __VERSION__ por la version real al vuelo. */
static esp_err_t h_root(httpd_req_t *req)
{
    touch_activity();
    const char *ver = XAMI_VERSION_STRING;
    const char *tpl = PORTAL_HTML;
    const char *mark = "__VERSION__";
    char *pos = strstr(tpl, mark);
    httpd_resp_set_type(req, "text/html");
    if (!pos) {
        httpd_resp_send(req, tpl, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    /* enviar en 3 trozos: antes, version, despues */
    httpd_resp_send_chunk(req, tpl, pos - tpl);
    httpd_resp_send_chunk(req, ver, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, pos + strlen(mark), HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* GET /scan -> escanea redes WiFi y devuelve JSON */
static esp_err_t h_scan(httpd_req_t *req)
{
    touch_activity();
    wifi_scan_config_t scan_cfg = { .show_hidden = false };
    esp_wifi_scan_start(&scan_cfg, true);

    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n > 20) n = 20;
    wifi_ap_record_t *recs = calloc(n, sizeof(wifi_ap_record_t));
    if (recs) esp_wifi_scan_get_ap_records(&n, recs);

    cJSON *root = cJSON_CreateObject();
    cJSON *arr  = cJSON_AddArrayToObject(root, "networks");
    for (int i = 0; recs && i < n; i++) {
        if (strlen((char*)recs[i].ssid) == 0) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "ssid", (char*)recs[i].ssid);
        cJSON_AddNumberToObject(o, "rssi", recs[i].rssi);
        cJSON_AddItemToArray(arr, o);
    }
    char *out = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    free(out); cJSON_Delete(root); if (recs) free(recs);
    return ESP_OK;
}

/* POST /connect -> guarda credenciales y reinicia */
static esp_err_t h_connect(httpd_req_t *req)
{
    touch_activity();
    char body[256];
    int len = req->content_len < (int)sizeof(body)-1 ? req->content_len : (int)sizeof(body)-1;
    int r = httpd_req_recv(req, body, len);
    if (r <= 0) { httpd_resp_send_500(req); return ESP_OK; }
    body[r] = 0;

    cJSON *root = cJSON_Parse(body);
    const char *ssid = root ? cJSON_GetStringValue(cJSON_GetObjectItem(root,"ssid")) : NULL;
    const char *pass = root ? cJSON_GetStringValue(cJSON_GetObjectItem(root,"pass")) : NULL;

    if (!ssid || strlen(ssid) == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"SSID requerido\"}", HTTPD_RESP_USE_STRLEN);
        if (root) cJSON_Delete(root);
        return ESP_OK;
    }

    esp_err_t err = wifi_provision_save(ssid, pass ? pass : "");
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
        ESP_LOGI(TAG, "Credenciales guardadas via portal — reiniciando en 2s");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"No se pudo guardar\"}", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

/* ---- DNS captivo: responde todo con 192.168.4.1 ---- */
#define DNS_PORT 53
#define AP_IP_ADDR 0x0104A8C0  /* 192.168.4.1 en network byte order */

static void dns_server_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { vTaskDelete(NULL); return; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DNS_PORT);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock); vTaskDelete(NULL); return;
    }

    uint8_t buf[512];
    struct sockaddr_in client;
    socklen_t clen = sizeof(client);

    while (s_running) {
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr*)&client, &clen);
        if (len < 12) continue;

        /* Construir respuesta DNS minima: copiar query, marcar respuesta,
           apuntar a 192.168.4.1 */
        buf[2] |= 0x80;  /* QR = respuesta */
        buf[3] |= 0x80;  /* RA */
        buf[7] = 1;      /* ANCOUNT = 1 */

        uint8_t ans[16];
        int p = 0;
        ans[p++] = 0xC0; ans[p++] = 0x0C;        /* puntero al nombre */
        ans[p++] = 0x00; ans[p++] = 0x01;        /* tipo A */
        ans[p++] = 0x00; ans[p++] = 0x01;        /* clase IN */
        ans[p++] = 0x00; ans[p++] = 0x00;
        ans[p++] = 0x00; ans[p++] = 0x3C;        /* TTL 60 */
        ans[p++] = 0x00; ans[p++] = 0x04;        /* RDLENGTH 4 */
        ans[p++] = 192; ans[p++] = 168; ans[p++] = 4; ans[p++] = 1;

        if (len + p <= (int)sizeof(buf)) {
            memcpy(buf + len, ans, p);
            sendto(sock, buf, len + p, 0,
                   (struct sockaddr*)&client, clen);
        }
    }
    close(sock);
    vTaskDelete(NULL);
}

/* ---- SoftAP + arranque del portal ---- */
static esp_err_t start_softap(void)
{
    /* Nombre del AP: Xami-<DeviceID> (consistente con la identidad del equipo) */
    char device_id[17];
    vault_get_device_id(device_id);
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "Xami-%s", device_id);

    wifi_config_t ap = {0};
    strncpy((char*)ap.ap.ssid, ap_ssid, sizeof(ap.ap.ssid)-1);
    ap.ap.ssid_len = strlen(ap_ssid);
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;   /* sin clave (se mejorara luego) */
    ap.ap.channel = 1;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP activo: %s (abierto) — portal en http://192.168.4.1", ap_ssid);
    return ESP_OK;
}

static httpd_handle_t start_portal_httpd(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 8;
    cfg.uri_match_fn = httpd_uri_match_wildcard;   /* captura todo */

    httpd_handle_t srv = NULL;
    if (httpd_start(&srv, &cfg) != ESP_OK) return NULL;

    httpd_uri_t u_scan = { .uri="/scan", .method=HTTP_GET, .handler=h_scan };
    httpd_uri_t u_conn = { .uri="/connect", .method=HTTP_POST, .handler=h_connect };
    httpd_uri_t u_root = { .uri="/*", .method=HTTP_GET, .handler=h_root };
    httpd_register_uri_handler(srv, &u_scan);
    httpd_register_uri_handler(srv, &u_conn);
    httpd_register_uri_handler(srv, &u_root);
    return srv;
}

esp_err_t captive_portal_start(void)
{
    ESP_LOGI(TAG, "Iniciando portal de configuracion Xami...");
    s_running = true;
    touch_activity();

    /* Inicializar base de WiFi de forma idempotente.
     * Si network_wifi_init() ya corrio, estas llamadas devuelven
     * INVALID_STATE y las ignoramos. Si no (caso sin credenciales),
     * las ejecutamos aqui. */
    esp_err_t e;
    e = esp_netif_init();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return e;
    e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return e;

    /* netif AP (puede ya existir el STA, no importa) */
    esp_netif_create_default_wifi_ap();

    /* wifi_init idempotente */
    wifi_init_config_t icfg = WIFI_INIT_CONFIG_DEFAULT();
    e = esp_wifi_init(&icfg);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_init: %s (probablemente ya init, continuando)", esp_err_to_name(e));
    }

    if (start_softap() != ESP_OK) return ESP_FAIL;

    s_portal_httpd = start_portal_httpd();
    if (!s_portal_httpd) { ESP_LOGE(TAG, "httpd fallo"); return ESP_FAIL; }

    xTaskCreate(dns_server_task, "dns_captive", 4096, NULL, 5, &s_dns_task);

    /* Loop de timeout: si nadie configura en CAPTIVE_PORTAL_TIMEOUT_SEC, salir */
    while (s_running) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        int64_t now = esp_timer_get_time() / 1000000;
        if (now - s_last_activity > CAPTIVE_PORTAL_TIMEOUT_SEC) {
            ESP_LOGW(TAG, "Portal timeout — sin actividad %d s", CAPTIVE_PORTAL_TIMEOUT_SEC);
            captive_portal_stop();
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_OK;
}

esp_err_t captive_portal_stop(void)
{
    s_running = false;
    if (s_portal_httpd) { httpd_stop(s_portal_httpd); s_portal_httpd = NULL; }
    ESP_LOGI(TAG, "Portal detenido");
    return ESP_OK;
}
