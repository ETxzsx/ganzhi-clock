#include <string.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "app.h"
#include "mini_json.h"
#include "web_server.h"

static const char *TAG = "web";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t setup_html_start[] asm("_binary_setup_html_start");
extern const uint8_t setup_html_end[] asm("_binary_setup_html_end");
extern const uint8_t core_js_start[] asm("_binary_core_js_start");
extern const uint8_t core_js_end[] asm("_binary_core_js_end");
extern const uint8_t astro_tables_js_start[] asm("_binary_astro_tables_js_start");
extern const uint8_t astro_tables_js_end[] asm("_binary_astro_tables_js_end");
extern const uint8_t liuyao_js_start[] asm("_binary_liuyao_js_start");
extern const uint8_t liuyao_js_end[] asm("_binary_liuyao_js_end");

static esp_err_t send_embedded(httpd_req_t *req, const uint8_t *start, const uint8_t *end, const char *type)
{
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, (const char *)start, end - start);
}

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t redirect_portal(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_sendstr(req, "");
}

static esp_err_t root_handler(httpd_req_t *req)
{
    if (app_mode() == DEV_SETUP)
        return send_embedded(req, setup_html_start, setup_html_end, "text/html; charset=utf-8");
    return send_embedded(req, index_html_start, index_html_end, "text/html; charset=utf-8");
}

static esp_err_t core_handler(httpd_req_t *req)
{
    return send_embedded(req, core_js_start, core_js_end, "application/javascript; charset=utf-8");
}
static esp_err_t astro_handler(httpd_req_t *req)
{
    return send_embedded(req, astro_tables_js_start, astro_tables_js_end, "application/javascript; charset=utf-8");
}
static esp_err_t liuyao_handler(httpd_req_t *req)
{
    return send_embedded(req, liuyao_js_start, liuyao_js_end, "application/javascript; charset=utf-8");
}

static esp_err_t now_handler(httpd_req_t *req)
{
    char buf[96];
    mj_t m;
    mj_init(&m, buf, sizeof buf);
    mj_obj_begin(&m);
    mj_kv_int(&m, "epoch", app_epoch_ms());
    mj_kv_bool(&m, "synced", app_synced());
    mj_obj_end(&m);
    return send_json(req, mj_done(&m));
}

static esp_err_t state_handler(httpd_req_t *req)
{
    char buf[320];
    mj_t m;
    mj_init(&m, buf, sizeof buf);
    mj_obj_begin(&m);
    mj_kv_str(&m, "mode", app_mode() == DEV_RUN ? "run" : "setup");
    mj_kv_int(&m, "view", app_view());
    mj_kv_str(&m, "ssid", app_ssid());
    mj_kv_str(&m, "ip", app_ip());
    mj_kv_int(&m, "rssi", app_rssi());
    mj_kv_bool(&m, "synced", app_synced());
    mj_kv_int(&m, "uptime", app_uptime_s());
    mj_kv_str(&m, "ver", APP_VER);
    mj_kv_str(&m, "mac", app_mac());
    mj_kv_bool(&m, "ap", app_ap_on());
    mj_obj_end(&m);
    return send_json(req, mj_done(&m));
}

static esp_err_t chart_handler(httpd_req_t *req)
{
    char buf[640];
    mj_t m;
    mj_init(&m, buf, sizeof buf);
    mj_obj_begin(&m);
    const ly_chart_t *c = app_chart();
    mj_kv_int(&m, "seq", app_cast_seq());
    mj_kv_bool(&m, "valid", c != NULL);
    if (c) {
        mj_kv_int(&m, "epoch", app_cast_epoch());
        mj_kv_int(&m, "dayIdx", app_cast_day_idx());
        mj_key(&m, "lines");
        mj_arr_begin(&m);
        for (int i = 0; i < 6; i++) {
            mj_obj_begin(&m);
            mj_kv_bool(&m, "yang", c->lines[i].yang);
            mj_kv_bool(&m, "moving", c->lines[i].moving);
            mj_kv_int(&m, "value", c->lines[i].value);
            mj_obj_end(&m);
        }
        mj_arr_end(&m);
    }
    mj_obj_end(&m);
    return send_json(req, mj_done(&m));
}

static int read_body(httpd_req_t *req, char *buf, int cap)
{
    int total = req->content_len;
    if (total <= 0 || total >= cap) return -1;
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, buf + got, total - got);
        if (r <= 0) return -1;
        got += r;
    }
    buf[got] = 0;
    return got;
}

static esp_err_t view_handler(httpd_req_t *req)
{
    char body[64];
    if (read_body(req, body, sizeof body) < 0)
        return send_json(req, "{\"ok\":false}");
    int v = -1;
    mj_get_int(body, "v", &v);
    if (v >= 0 && v <= 2) app_set_view(v);
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t reconfig_handler(httpd_req_t *req)
{
    app_cmd_reconfig();
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t ap_handler(httpd_req_t *req)
{
    char body[32];
    if (read_body(req, body, sizeof body) < 0)
        return send_json(req, "{\"ok\":false}");
    int on = 0;
    mj_get_int(body, "on", &on);
    app_cmd_ap(on != 0);
    return send_json(req, "{\"ok\":true}");
}

static wifi_ap_record_t s_scan[24];
static uint16_t s_scan_n = 0;
static int64_t s_scan_at = -100000000;

static esp_err_t scan_handler(httpd_req_t *req)
{
    int64_t now = esp_timer_get_time();
    if (now - s_scan_at > 5000000) {
        s_scan_n = 0;
        if (esp_wifi_scan_start(NULL, true) == ESP_OK) {
            uint16_t n = 24;
            if (esp_wifi_scan_get_ap_records(&n, s_scan) == ESP_OK) s_scan_n = n;
            esp_wifi_scan_get_ap_num(&n);
        }
        s_scan_at = now;
    }
    static char buf[3072];
    mj_t m;
    mj_init(&m, buf, sizeof buf);
    mj_arr_begin(&m);
    for (int i = 0; i < s_scan_n; i++) {
        if (!s_scan[i].ssid[0]) continue;
        int dup = 0;
        for (int j = 0; j < i; j++)
            if (strcmp((char *)s_scan[j].ssid, (char *)s_scan[i].ssid) == 0) { dup = 1; break; }
        if (dup) continue;
        mj_obj_begin(&m);
        mj_kv_str(&m, "ssid", (char *)s_scan[i].ssid);
        mj_kv_int(&m, "rssi", s_scan[i].rssi);
        mj_kv_bool(&m, "open", s_scan[i].authmode == WIFI_AUTH_OPEN);
        mj_obj_end(&m);
    }
    mj_arr_end(&m);
    return send_json(req, mj_done(&m));
}

static esp_err_t connect_handler(httpd_req_t *req)
{
    char body[256];
    if (read_body(req, body, sizeof body) < 0)
        return send_json(req, "{\"ok\":false,\"reason\":\"请求无效\"}");
    char ssid[33] = "", pass[65] = "";
    mj_get_str(body, "ssid", ssid, sizeof ssid);
    mj_get_str(body, "pass", pass, sizeof pass);
    if (!ssid[0])
        return send_json(req, "{\"ok\":false,\"reason\":\"请输入网络名称\"}");
    app_cmd_connect(ssid, pass);
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char buf[256];
    mj_t m;
    mj_init(&m, buf, sizeof buf);
    mj_obj_begin(&m);
    const char *st = "idle";
    switch (app_prov()) {
    case PROV_CONNECTING: st = "connecting"; break;
    case PROV_OK: st = "ok"; break;
    case PROV_FAIL: st = "fail"; break;
    default: break;
    }
    mj_kv_str(&m, "state", st);
    mj_kv_str(&m, "ssid", app_ssid());
    mj_kv_str(&m, "ip", app_ip());
    mj_kv_str(&m, "reason", app_fail_reason());
    mj_kv_str(&m, "mac", app_mac());
    mj_obj_end(&m);
    return send_json(req, mj_done(&m));
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (app_mode() == DEV_SETUP) return redirect_portal(req);
    return send_embedded(req, index_html_start, index_html_end, "text/html; charset=utf-8");
}

#define GET(u, h) do { httpd_uri_t x = { .uri = u, .method = HTTP_GET, .handler = h }; httpd_register_uri_handler(srv, &x); } while (0)
#define POST(u, h) do { httpd_uri_t x = { .uri = u, .method = HTTP_POST, .handler = h }; httpd_register_uri_handler(srv, &x); } while (0)

void web_server_start(void)
{
    httpd_handle_t srv = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.stack_size = 8192;
    cfg.max_open_sockets = 7;
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 32;
    if (httpd_start(&srv, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed");
        return;
    }
    GET("/", root_handler);
    GET("/index.html", root_handler);
    GET("/core.js", core_handler);
    GET("/astro_tables.js", astro_handler);
    GET("/liuyao.js", liuyao_handler);
    GET("/api/now", now_handler);
    GET("/api/state", state_handler);
    GET("/api/chart", chart_handler);
    POST("/api/view", view_handler);
    POST("/api/reconfig", reconfig_handler);
    POST("/api/ap", ap_handler);
    GET("/scan", scan_handler);
    POST("/connect", connect_handler);
    GET("/status", status_handler);
    GET("/favicon.ico", favicon_handler);
    GET("/generate_204", redirect_portal);
    GET("/gen_204", redirect_portal);
    GET("/hotspot-detect.html", redirect_portal);
    GET("/library/test/success.html", redirect_portal);
    GET("/success.txt", redirect_portal);
    GET("/connecttest.txt", redirect_portal);
    GET("/ncsi.txt", redirect_portal);
    GET("/redirect", redirect_portal);
    GET("/fwlink", redirect_portal);
    httpd_register_err_handler(srv, HTTPD_404_NOT_FOUND, not_found_handler);
    ESP_LOGI(TAG, "http server ready");
}