#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "app.h"
#include "dns_portal.h"
#include "mdns_mini.h"
#include "web_server.h"
#include "xl9555.h"
#include "ssd1306.h"
#include "st7789.h"
#include "keys.h"
#include "ui.h"

static const char *TAG = "ganzhi";
#define NVS_NS "gzcfg"
#define NVS_TIME "gztime"
#define DEFAULT_EPOCH 1767225600000LL   /* 2026-01-01 00:00 UTC，未校时时的占位 */

static dev_mode_t s_mode = DEV_SETUP;
static volatile int s_view = 0;
static char s_ssid[33];
static char s_pass[65];
static char s_ip[16] = "";
static volatile int s_rssi = 0;
static volatile bool s_synced = false;
static volatile bool s_connected = false;
static prov_state_t s_prov = PROV_IDLE;
static char s_reason[80] = "";
static char s_mac[18] = "";
static int64_t s_prov_ok_at = 0;
static int s_fail_count = 0;
static int64_t s_next_retry_us = 0;
static esp_netif_t *s_sta_netif;
static bool s_mdns_on = false;
static volatile bool s_ap_on = false;

/* 离线时间基准：未校时时用 NVS 恢复值 + 运行时长推算 */
static int64_t s_base_epoch = DEFAULT_EPOCH;
static int64_t s_boot_us = 0;
static bool s_time_valid = false;
static int64_t s_last_save_us = 0;

/* 显示与六爻状态 */
static volatile bool s_invert = false;
static volatile int s_bright = 2;
static volatile bool s_sleep = false;
static volatile uint32_t s_cast_seq = 0;
static ly_chart_t s_chart;
static bool s_chart_valid = false;
static int64_t s_cast_epoch;
static int s_cast_day;

typedef struct {
    int type;          /* 1 进入配网 2 连接WiFi 3 按键 4 退出配网 */
    char ssid[33];
    char pass[65];
    int key, ev;
} cmd_t;
static QueueHandle_t s_cmdq;

dev_mode_t app_mode(void) { return s_mode; }
int app_view(void) { return s_view; }
void app_set_view(int v) { if (v >= 0 && v <= 2) s_view = v; }
const char *app_ssid(void) { return s_ssid; }
const char *app_ip(void) { return s_ip; }
int app_rssi(void) { return s_rssi; }
bool app_synced(void) { return s_synced; }
bool app_time_valid(void) { return s_time_valid || s_synced; }
prov_state_t app_prov(void) { return s_prov; }
const char *app_fail_reason(void) { return s_reason; }
const char *app_mac(void) { return s_mac; }
bool app_ap_on(void) { return s_ap_on; }
uint32_t app_uptime_s(void) { return (uint32_t)(esp_timer_get_time() / 1000000); }
bool app_invert(void) { return s_invert; }
int app_brightness(void) { return s_bright; }
bool app_display_sleep(void) { return s_sleep; }
uint32_t app_cast_seq(void) { return s_cast_seq; }
int64_t app_cast_epoch(void) { return s_cast_epoch; }
int app_cast_day_idx(void) { return s_cast_day; }
const ly_chart_t *app_chart(void) { return s_chart_valid ? &s_chart : NULL; }

int64_t app_epoch_ms(void)
{
    if (s_synced) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }
    return s_base_epoch + (esp_timer_get_time() - s_boot_us) / 1000;
}

void app_cmd_reconfig(void)
{
    cmd_t c = { .type = 1 };
    xQueueSend(s_cmdq, &c, 0);
}
void app_cmd_connect(const char *ssid, const char *pass)
{
    cmd_t c = { .type = 2 };
    strlcpy(c.ssid, ssid, sizeof c.ssid);
    strlcpy(c.pass, pass, sizeof c.pass);
    xQueueSend(s_cmdq, &c, pdMS_TO_TICKS(100));
}
void app_cmd_key(int key, int ev)
{
    cmd_t c = { .type = 3, .key = key, .ev = ev };
    xQueueSend(s_cmdq, &c, 0);
}
void app_cmd_ap(bool on)
{
    cmd_t c = { .type = 5, .key = on ? 1 : 0 };
    xQueueSend(s_cmdq, &c, 0);
}
static void cmd_exit_setup(void)
{
    cmd_t c = { .type = 4 };
    xQueueSend(s_cmdq, &c, 0);
}

static void save_creds(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "ssid", s_ssid);
        nvs_set_str(h, "pass", s_pass);
        nvs_commit(h);
        nvs_close(h);
    }
}
static bool load_creds(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sl = sizeof s_ssid, pl = sizeof s_pass;
    esp_err_t e1 = nvs_get_str(h, "ssid", s_ssid, &sl);
    nvs_get_str(h, "pass", s_pass, &pl);
    nvs_close(h);
    return e1 == ESP_OK && s_ssid[0];
}
static void save_time(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_TIME, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i64(h, "epoch", app_epoch_ms());
        nvs_commit(h);
        nvs_close(h);
    }
}
static void restore_time(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_TIME, NVS_READONLY, &h) != ESP_OK) return;
    int64_t e = 0;
    if (nvs_get_i64(h, "epoch", &e) == ESP_OK && e > 1700000000000LL) {
        time_t sec = (time_t)(e / 1000);
        struct tm tm;
        gmtime_r(&sec, &tm);
        if (tm.tm_year + 1900 >= 2026 && tm.tm_year + 1900 < 2100) {
            s_base_epoch = e;
            s_time_valid = true;
            ESP_LOGI(TAG, "time restored from nvs");
        }
    }
    nvs_close(h);
}

static void start_sntp(void)
{
    static bool started = false;
    if (started) return;
    started = true;
    setenv("TZ", "CST-8", 1);
    tzset();
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(4,
        ESP_SNTP_SERVER_LIST("ntp.aliyun.com", "ntp.tencent.com", "cn.pool.ntp.org", "pool.ntp.org"));
    cfg.start = true;
    cfg.renew_servers_after_new_IP = true;
    cfg.sync_cb = NULL;
    esp_netif_sntp_init(&cfg);
    ESP_LOGI(TAG, "sntp started");
}

static void mdns_up(void)
{
    if (s_mdns_on) return;
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(s_sta_netif, &info) == ESP_OK) {
        ip4_addr_t a;
        a.addr = info.ip.addr;
        mdns_mini_start("ganzhi", &a);
        s_mdns_on = true;
    }
}

/* 运行中内网热点：AP+STA 共存，家用 WiFi 与校时不中断；手机连 AP 看伴侣页 */
static void ap_share_start(void)
{
    wifi_ap_record_t info;
    int ch = 1;
    if (esp_wifi_sta_get_ap_info(&info) == ESP_OK) ch = info.primary;
    wifi_config_t ap = { 0 };
    strlcpy((char *)ap.ap.ssid, AP_SSID, sizeof ap.ap.ssid);
    ap.ap.ssid_len = strlen(AP_SSID);
    ap.ap.channel = ch;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.max_connection = 4;
    ap.ap.beacon_interval = 100;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    dns_portal_start();
    s_ap_on = true;
    ESP_LOGI(TAG, "intranet AP on (ch%d), sta kept", ch);
}
static void ap_share_stop(void)
{
    dns_portal_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    s_ap_on = false;
    ESP_LOGI(TAG, "intranet AP off");
}

static void enter_setup(void)
{
    ESP_LOGI(TAG, "enter setup mode");
    s_mode = DEV_SETUP;
    s_ap_on = false;
    s_prov = PROV_IDLE;
    s_connected = false;
    s_ip[0] = 0;
    if (s_mdns_on) { mdns_mini_stop(); s_mdns_on = false; }
    esp_wifi_stop();
    wifi_config_t ap = { 0 };
    strlcpy((char *)ap.ap.ssid, AP_SSID, sizeof ap.ap.ssid);
    ap.ap.ssid_len = strlen(AP_SSID);
    ap.ap.channel = 1;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.max_connection = 4;
    ap.ap.beacon_interval = 100;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    dns_portal_start();
}

static void to_run(void)
{
    ESP_LOGI(TAG, "run mode, ip=%s", s_ip);
    dns_portal_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    s_mode = DEV_RUN;
    s_ap_on = false;
    s_prov = PROV_IDLE;
    start_sntp();
    if (s_connected) mdns_up();
    else esp_wifi_connect();
}

static void set_sta_config(void)
{
    wifi_config_t sta = { 0 };
    strlcpy((char *)sta.sta.ssid, s_ssid, sizeof sta.sta.ssid);
    strlcpy((char *)sta.sta.password, s_pass, sizeof sta.sta.password);
    sta.sta.threshold.authmode = s_pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    sta.sta.pmf_cfg.required = false;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
}

static void start_sta_connect(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    set_sta_config();
    ESP_ERROR_CHECK(esp_wifi_start());   /* STA_START 事件里发起连接，避免重复 connect */
}

static const char *reason_text(uint8_t r)
{
    switch (r) {
    case WIFI_REASON_NO_AP_FOUND: return "未找到该网络";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "密码错误或认证失败";
    case WIFI_REASON_ASSOC_FAIL: return "路由器拒绝接入";
    case WIFI_REASON_BEACON_TIMEOUT: return "信号不佳连接超时";
    default: return "连接失败请重试";
    }
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            if (s_mode == DEV_RUN) esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *d = data;
            s_connected = false;
            s_rssi = 0;
            s_ip[0] = 0;
            if (s_prov == PROV_CONNECTING) {
                s_prov = PROV_FAIL;
                snprintf(s_reason, sizeof s_reason, "%s", reason_text(d->reason));
                ESP_LOGW(TAG, "prov fail: %s (reason %d)", s_reason, d->reason);
            } else if (s_mode == DEV_RUN) {
                if (s_fail_count < 100) s_fail_count++;
                int back = 1 << (s_fail_count < 5 ? s_fail_count : 5);
                if (back > 30) back = 30;
                s_next_retry_us = esp_timer_get_time() + (int64_t)back * 1000000;
            }
            break;
        }
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "phone joined AP");
            if (s_connected) { start_sntp(); esp_netif_sntp_start(); }  /* 手机接入：立即校时一次 */
            break;
        default: break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        snprintf(s_ip, sizeof s_ip, IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_fail_count = 0;
        ESP_LOGI(TAG, "got ip %s", s_ip);
        if (s_prov == PROV_CONNECTING) {
            s_prov = PROV_OK;
            s_prov_ok_at = esp_timer_get_time();
        }
        if (s_mode == DEV_RUN) {
            start_sntp();
            esp_netif_sntp_start();   /* 自动联网成功：立即校时一次 */
            mdns_up();
        }
    }
}

static void do_cast(void)
{
    gz_now_t n;
    gz_compute(app_epoch_ms(), &n);
    ly_line_t lines[6];
    ly_cast(lines);
    ly_chart(lines, n.dIdx % 10, n.dIdx, &s_chart);
    s_chart_valid = true;
    s_cast_seq++;
    s_cast_epoch = app_epoch_ms();
    s_cast_day = n.dIdx;
    ESP_LOGI(TAG, "cast %s -> %s", s_chart.name, s_chart.changedName);
}

static void handle_key(int k, int ev)
{
    if (s_sleep) {
        s_sleep = false;   /* 休眠中任意键唤醒 */
        return;
    }
    if (k == KEY_BOOT) {
        if (ev == KEV_SHORT) {
            s_view = (s_view + 1) % 3;
            ESP_LOGI(TAG, "view %d", s_view);
        } else {
            if (s_mode == DEV_RUN) app_cmd_ap(!s_ap_on);
            else if (s_ssid[0]) cmd_exit_setup();
        }
    } else if (k == KEY_0 && ev == KEV_SHORT) {
        s_view = 1;
        do_cast();
    } else if (k == KEY_1 && ev == KEV_SHORT) {
        s_invert = !s_invert;
    } else if (k == KEY_2 && ev == KEV_SHORT) {
        s_bright = (s_bright + 1) & 3;
    } else if (k == KEY_3) {
        if (ev == KEV_SHORT) {
            s_view = 2;
        } else {
            s_sleep = true;
        }
    }
}

static void control_task(void *arg)
{
    cmd_t c;
    int64_t last_rssi = 0;
    for (;;) {
        while (xQueueReceive(s_cmdq, &c, 0) == pdTRUE) {
            if (c.type == 1) enter_setup();
            else if (c.type == 4) to_run();
            else if (c.type == 5) {
                if (s_mode == DEV_RUN) {
                    if (c.key && !s_ap_on) ap_share_start();
                    else if (!c.key && s_ap_on) ap_share_stop();
                }
            }
            else if (c.type == 3) handle_key(c.key, c.ev);
            else if (c.type == 2) {
                strlcpy(s_ssid, c.ssid, sizeof s_ssid);
                strlcpy(s_pass, c.pass, sizeof s_pass);
                save_creds();
                set_sta_config();
                s_prov = PROV_CONNECTING;
                s_reason[0] = 0;
                ESP_LOGI(TAG, "connecting to %s", s_ssid);
                esp_wifi_disconnect();
                esp_wifi_connect();
            }
        }
        int64_t now = esp_timer_get_time();
        if (s_prov == PROV_OK && s_mode == DEV_SETUP && now - s_prov_ok_at > 3000000)
            to_run();
        if (s_mode == DEV_RUN && !s_connected && now >= s_next_retry_us) {
            s_next_retry_us = now + 30000000;
            esp_wifi_connect();
        }
        if (s_connected && now - last_rssi > 5000000) {
            last_rssi = now;
            wifi_ap_record_t ap;
            if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) s_rssi = ap.rssi;
        }
        if (!s_synced && s_connected && esp_netif_sntp_sync_wait(0) == ESP_OK) {
            s_synced = true;
            s_time_valid = true;
            save_time();
            ESP_LOGI(TAG, "time synced");
        }
        static int64_t s_retry_at;
        if (!s_synced && s_connected && app_epoch_ms() - s_retry_at > 15000) {
            s_retry_at = app_epoch_ms();
            esp_netif_sntp_start();   /* 未校准时 15s 重试，避免首败后等 1 小时 */
        }
        if (s_synced && now - s_last_save_us > 600000000) {
            s_last_save_us = now;
            save_time();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void key_cb(int key, int ev) { app_cmd_key(key, ev); }

static void self_test(void)
{
    gz_now_t n;
    gz_compute(1794917820000LL, &n);   /* 2026-11-17 20:17 CST */
    char y[7], m[7], d[7], h[7];
    gz_pillar(n.yIdx, y); gz_pillar(n.mIdx, m);
    gz_pillar(n.dIdx, d); gz_pillar(n.hIdx, h);
    bool ok = !strcmp(y, "丙午") && !strcmp(m, "己亥") && !strcmp(d, "乙未") && !strcmp(h, "丙戌");
    ESP_LOGI(TAG, "self-test %s: %s %s %s %s", ok ? "PASS" : "FAIL", y, m, d, h);
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_mac, sizeof s_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    s_boot_us = esp_timer_get_time();
    s_next_retry_us = s_boot_us + 30000000;   /* 首连由 STA_START 发起，重试定时器稍后再介入 */
    s_cmdq = xQueueCreate(8, sizeof(cmd_t));
    restore_time();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL, NULL));

    web_server_start();

    if (xl9555_init() == ESP_OK) {
        if (ssd1306_init() == ESP_OK) {
            ESP_LOGI(TAG, "display: OLED 128x64 on P6");
        } else if (st7789_init() == ESP_OK) {
            ssd1306_attach_lcd();
            ESP_LOGI(TAG, "display: SPI LCD ST7789 on WIRELESS");
        } else {
            ESP_LOGE(TAG, "no display module detected (P6 OLED / WIRELESS LCD)");
        }
    } else ESP_LOGE(TAG, "xl9555 missing, displays disabled");
    ui_start();
    keys_start(key_cb);
    xTaskCreate(control_task, "ctrl", 8192, NULL, 6, NULL);
    self_test();

    if (load_creds()) {
        ESP_LOGI(TAG, "saved wifi: %s -> run", s_ssid);
        s_mode = DEV_RUN;
        start_sta_connect();      /* 非阻塞，后台连接 */
    } else {
        ESP_LOGI(TAG, "no credentials -> setup");
        enter_setup();
    }
}