#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gz_core.h"

#define APP_VER "1.1.0"
#define AP_SSID "GanZhi-Setup"

typedef enum { DEV_SETUP, DEV_RUN } dev_mode_t;
typedef enum { PROV_IDLE, PROV_CONNECTING, PROV_OK, PROV_FAIL } prov_state_t;

dev_mode_t app_mode(void);
int app_view(void);
void app_set_view(int v);
const char *app_ssid(void);
const char *app_ip(void);
int app_rssi(void);
bool app_synced(void);
bool app_time_valid(void);
prov_state_t app_prov(void);
const char *app_fail_reason(void);
const char *app_mac(void);
int64_t app_epoch_ms(void);
uint32_t app_uptime_s(void);
void app_cmd_reconfig(void);
void app_cmd_connect(const char *ssid, const char *pass);
void app_cmd_key(int key, int ev);
void app_cmd_ap(bool on);      /* 运行中开/关内网热点（AP+STA 共存） */
bool app_ap_on(void);

/* OLED 显示状态 */
bool app_invert(void);
int app_brightness(void);      /* 0..3 */
bool app_display_sleep(void);
uint32_t app_cast_seq(void);
int64_t app_cast_epoch(void);
int app_cast_day_idx(void);
const ly_chart_t *app_chart(void);