#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "app.h"
#include "ssd1306.h"
#include "st7789.h"
#include "gz_core.h"
#include "ui.h"

#define TAG "ui"
static const uint8_t BRIGHT[4] = { 0x30, 0x80, 0xCF, 0xFF };
#define ANIM_US 900000

static gz_now_t s_now;
static int64_t s_anim_start = -1;
static uint32_t s_last_seq;
static bool s_lcd;

/* ---------- LCD 原生 240x320 彩色视图（安全区 y80..239） ---------- */
static const int GAN_WX[10] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4 };
static const int ZHI_WX[12] = { 4, 2, 0, 0, 2, 1, 1, 2, 3, 3, 2, 4 };
static const int WX_C[5] = { LC_JADE, LC_CINN, LC_EARTH, LC_SILVER, LC_INDIGO };
static const char WX_N[5][4] = { "木", "火", "土", "金", "水" };
static const int REL_C[5] = { LC_JADE, LC_GOLD2, LC_INDIGO, LC_GOLD, LC_CINN2 };
static const int SPI_C[6] = { LC_JADE, LC_CINN, LC_EARTH, LC_VIOLET, LC_SILVER, LC_INDIGO };

static void lc16(int y, const char *s, int c)
{
    lcd_text16((240 - lcd_text16_width(s)) / 2, y, s, c);
}
static void lc16s(int y, const char *s, int c)
{
    lcd_text16s((240 - lcd_text16_width(s) / 2) / 2, y, s, c);
}
static void lc8(int y, const char *s, int c, int sc)
{
    lcd_text8((240 - (int)strlen(s) * 8 * sc) / 2, y, s, c, sc);
}

static void render_splash_lcd(void)
{
    lcd_clear(LC_BG);
    lcd_decor();
    lc16(120, "干支钟", LC_GOLD);
    lc8(146, "GANZHI CLOCK", LC_MUT, 1);
    lc8(164, APP_VER, LC_GOLD2, 1);
    lc16s(186, "启动中", LC_GOLD2);
    lcd_flush();
}

static void render_clock_lcd(void)
{
    lcd_clear(LC_BG);
    lcd_decor();
    static const char *LBL[4] = { "年柱", "月柱", "日柱", "时柱" };
    const int idx[4] = { s_now.yIdx, s_now.mIdx, s_now.dIdx, s_now.hIdx };
    for (int i = 0; i < 4; i++) {
        int x = 14 + i * 56;
        lcd_text16s(x, 82, LBL[i], LC_MUT);
        lcd_text16(x + 2, 94, gz_gan(idx[i] % 10), WX_C[GAN_WX[idx[i] % 10]]);
        lcd_text16(x + 22, 94, gz_zhi(idx[i] % 12), WX_C[ZHI_WX[idx[i] % 12]]);
    }
    lcd_rect(14, 114, 212, 1, LC_LINE);
    if (app_ap_on()) lcd_text16s(220, 82, "热", LC_GOLD2);
    char t[12];
    snprintf(t, sizeof t, "%02d:%02d:%02d", s_now.h, s_now.mi, s_now.s);
    lc8(120, t, LC_GOLD2, 2);
    char l[64];
    snprintf(l, sizeof l, "%04d-%02d-%02d %s", s_now.Y, s_now.M, s_now.D, gz_week(s_now.dow));
    lc16s(156, l, LC_MUT);
    snprintf(l, sizeof l, "农历%s%s%s %s时", s_now.lunarLeap ? "闰" : "",
             gz_yue(s_now.lunarM), gz_ri(s_now.lunarD), gz_zhi(s_now.hBranch));
    lc16s(168, l, LC_SILK);
    lcd_text16s(220, 168, app_synced() ? "校" : "未", app_synced() ? LC_JADE : LC_MUT);
    snprintf(l, sizeof l, "%s", gz_shichen_range(s_now.hBranch));
    lcd_text16s(14, 180, l, LC_MUT);
    const char *ny = gz_nayin(s_now.dIdx);
    lcd_text16s(226 - lcd_text16_width(ny) / 2, 180, ny, LC_GOLD);
    snprintf(l, sizeof l, "%s 至 %s", gz_term(s_now.termCurK), gz_term(s_now.termNextK));
    lc16s(192, l, s_now.jieqiDay ? LC_GOLD2 : LC_SILK);
    lcd_frame(14, 204, 212, 5, LC_LINE);
    int64_t span = s_now.termNextMs - s_now.termCurMs;
    int pct = span > 0 ? (int)((s_now.epoch - s_now.termCurMs) * 210 / span) : 0;
    if (pct < 0) pct = 0;
    if (pct > 210) pct = 210;
    lcd_rect(15, 205, pct, 3, LC_GOLD);
    int cnt[5] = { 0 };
    for (int i = 0; i < 4; i++) {
        cnt[GAN_WX[idx[i] % 10]]++;
        cnt[ZHI_WX[idx[i] % 12]]++;
    }
    int x = 14;
    for (int e = 0; e < 5; e++) {
        if (cnt[e]) lcd_rect(x, 212, cnt[e] * 26, 6, WX_C[e]);
        x += cnt[e] * 26;
    }
    x = 14;
    for (int e = 0; e < 5; e++) {
        snprintf(l, sizeof l, "%s%d", WX_N[e], cnt[e]);
        x += lcd_text16s(x, 220, l, WX_C[e]) + 6;
    }
    snprintf(l, sizeof l, "旬空 %s%s", gz_zhi(s_now.kong[0]), gz_zhi(s_now.kong[1]));
    lcd_text16s(14, 230, l, LC_MUT);
    snprintf(l, sizeof l, "甲子 %d", s_now.dIdx);
    lcd_text16s(226 - lcd_text16_width(l) / 2, 230, l, LC_GOLD);
    lcd_flush();
}

static void render_liuyao_lcd(int64_t now_us)
{
    lcd_clear(LC_BG);
    lcd_decor();
    const ly_chart_t *c = app_chart();
    uint32_t seq = app_cast_seq();
    if (seq != s_last_seq) { s_last_seq = seq; s_anim_start = now_us; }
    int64_t el = (s_anim_start < 0) ? ANIM_US : now_us - s_anim_start;
    int animating = c && el < ANIM_US;
    char h[64];

    if (!c) {
        lc16(140, "按键起卦", LC_GOLD);
        lc16s(166, "KEY0 短按起卦", LC_MUT);
        lc16s(182, "BOOT 切换界面", LC_MUT);
        lcd_rect(90, 204, 60, 4, LC_LINE);
        lcd_rect(90, 212, 26, 4, LC_LINE);
        lcd_rect(124, 212, 26, 4, LC_LINE);
        lcd_rect(90, 220, 60, 4, LC_LINE);
        lcd_flush();
        return;
    }
    if (animating) {
        snprintf(h, sizeof h, "起卦中 %d%%", (int)(el * 100 / ANIM_US));
        lc16s(84, h, LC_GOLD2);
    } else if (c->hasMoving) {
        snprintf(h, sizeof h, "%s 之 %s", c->name, c->changedName);
        lc16(82, h, LC_GOLD);
    } else {
        snprintf(h, sizeof h, "%s 静卦", c->name);
        lc16(82, h, LC_GOLD);
    }
    snprintf(h, sizeof h, "%s宫 %s", ly_palace_name(c->palace), ly_stage_name(c->stage));
    lc16s(100, h, LC_MUT);

    for (int i = 0; i < 6; i++) {
        int li = 5 - i;
        int y = 112 + i * 18;
        const ly_line_t *ln = &c->lines[li];
        int bar_c = ln->moving ? LC_CINN2 : LC_GOLD;
        int phase = -1;
        if (animating && el < (int64_t)(li + 1) * 140000) phase = (int)(el / 50000);
        if (phase >= 0) {
            lcd_circle(38, y + 7, 5, 0, LC_GOLD2);
            float a = (float)phase * 0.35f + li;
            lcd_pixel(38 + (int)(cosf(a) * 3), y + 7 + (int)(sinf(a) * 3), LC_CINN2);
            lcd_text16s(92, y + 4, "…", LC_LINE);
            continue;
        }
        if (ln->yang) lcd_rect(14, y + 5, 60, 5, bar_c);
        else { lcd_rect(14, y + 5, 26, 5, bar_c); lcd_rect(48, y + 5, 26, 5, bar_c); }
        if (ln->moving) lcd_text8(78, y + 2, ln->yang ? "O" : "X", LC_CINN2, 1);
        lcd_text16s(92, y + 4, gz_zhi(c->zhiIdx[li]), WX_C[ZHI_WX[c->zhiIdx[li] % 12]]);
        lcd_text16s(102, y + 4, ly_rel(c->rel[li]), REL_C[c->rel[li] % 5]);
        lcd_text16s(120, y + 4, ly_spirit(c->spirit[li]), SPI_C[c->spirit[li] % 6]);
        if (li == c->shi) lcd_text16s(140, y + 4, "世", LC_GOLD);
        if (li == c->ying) lcd_text16s(150, y + 4, "应", LC_INDIGO);
        if (c->kong[0] == c->zhiIdx[li] || c->kong[1] == c->zhiIdx[li])
            lcd_text16s(160, y + 4, "空", LC_MUT);
        if (ln->moving) {
            lcd_text16s(176, y + 4, gz_zhi(c->changedZhiIdx[li]), LC_CINN2);
            lcd_text16s(186, y + 4, "变", LC_MUT);
        }
    }
    snprintf(h, sizeof h, "旬空 %s%s", gz_zhi(c->kong[0]), gz_zhi(c->kong[1]));
    lcd_text16s(14, 226, h, LC_MUT);
    lcd_text16s(186, 226, "KEY0 起卦", LC_MUT);
    lcd_flush();
}

static void render_status_lcd(void)
{
    lcd_clear(LC_BG);
    lcd_decor();
    bool online = app_ip()[0] != 0;
    lcd_text16(14, 82, "网络状态", LC_GOLD);
    lcd_text16s(226 - lcd_text16_width(APP_VER) / 2, 86, APP_VER, LC_MUT);
    char v[64];
    const char *ssid = app_mode() == DEV_SETUP ? AP_SSID : app_ssid();

    lcd_text16s(14, 102, "网络", LC_MUT);
    if (ssid[0]) lcd_text16s(46, 102, ssid, app_mode() == DEV_SETUP ? LC_GOLD2 : LC_SILK);
    else lcd_text16s(46, 102, "未配置", LC_MUT);

    lcd_text16s(14, 118, "状态", LC_MUT);
    prov_state_t pv = app_prov();
    if (pv == PROV_OK) lcd_text16s(46, 118, "已连接", LC_JADE);
    else if (pv == PROV_CONNECTING) lcd_text16s(46, 118, "连接中", LC_GOLD2);
    else if (pv == PROV_FAIL) lcd_text16s(46, 118, "失败", LC_CINN2);
    else lcd_text16s(46, 118, app_mode() == DEV_SETUP ? "热点配网" : (online ? "已连接" : "离线运行"),
                     app_mode() == DEV_SETUP ? LC_GOLD2 : (online ? LC_JADE : LC_MUT));

    lcd_text16s(14, 134, "校时", LC_MUT);
    if (app_synced()) {
        snprintf(v, sizeof v, "已校时-%s", strcmp(app_time_src(), "phone") == 0 ? "手机" : "网络");
        lcd_text16s(46, 134, v, LC_JADE);
    }
    else if (app_time_valid()) lcd_text16s(46, 134, "运行中", LC_GOLD2);
    else lcd_text16s(46, 134, "未校时", LC_MUT);

    lcd_text16s(14, 150, "地址", LC_MUT);
    lcd_text16s(46, 150, online ? app_ip() : "0.0.0.0", LC_SILK);
    int rssi = app_rssi();
    int n = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
    if (!online) n = 0;
    int bc = n >= 3 ? LC_JADE : n == 2 ? LC_GOLD2 : LC_CINN2;
    for (int k = 0; k < 4; k++) {
        int bh = 4 + k * 3;
        lcd_rect(204 + k * 6, 156 - bh, 4, bh, k < n ? bc : LC_LINE);
    }

    lcd_text16s(14, 166, "运行", LC_MUT);
    snprintf(v, sizeof v, "UP %lus", (unsigned long)app_uptime_s());
    lcd_text16s(46, 166, v, LC_SILK);
    snprintf(v, sizeof v, "%.2f,%.2f", app_lat_e6() / 1e6, app_lon_e6() / 1e6);
    lcd_text16s(120, 166, v, LC_MUT);
    lcd_text16s(14, 182, "MAC", LC_MUT);
    lcd_text16s(46, 182, app_mac(), LC_MUT);

    if (pv == PROV_FAIL) {
        snprintf(v, sizeof v, "%.24s", app_fail_reason());
        lcd_text16s(14, 198, v, LC_CINN2);
    } else if (pv == PROV_CONNECTING) {
        lc16s(198, "连接中 CONNECTING", LC_GOLD2);
    } else if (app_ap_on()) {
        lc16s(198, "内网热点 GanZhi-Setup", LC_GOLD2);
    } else if (app_mode() == DEV_SETUP) {
        lc16s(198, "手机连热点 浏览器配网", LC_GOLD2);
    }
    lcd_text16s(14, 214, "BOOT 切换 / 长按配网", LC_MUT);
    lcd_text16s(14, 228, "K0卦 K1版式 K2亮度 K3状态", LC_MUT);
    lcd_flush();
}

/* ---------- 星象：盖天轮 上北下南左西右东 ---------- */
static void render_star_lcd(void)
{
    lcd_clear(LC_BG);
    lcd_decor();
    gz_star_t st;
    gz_star(s_now.epoch, app_lon_e6() / 1e6, &st);
    const int CX = 120, CY = 160;
    const double D2R = 3.14159265358979323846 / 180.0;
    static const int QUAD_C[4] = { LC_JADE, LC_INDIGO, LC_SILVER, LC_CINN };
    lcd_circle(CX, CY, 60, 0, LC_LINE);
    lcd_circle(CX, CY, 42, 0, LC_LINE);
    lcd_circle(CX, CY, 22, 0, LC_LINE);
    for (int k = 0; k < 28; k++) {
        double s0, w;
        gz_lodge_disp(k, &s0, &w);
        double b = s0 * D2R;
        for (int r = 42; r <= 60; r++)
            lcd_pixel(CX + (int)(cos(b) * r), CY - (int)(sin(b) * r), LC_LINE);
        double m = (s0 + w / 2) * D2R;
        int lr = w < 6 ? 56 : 50;
        lcd_text16s(CX + (int)(cos(m) * lr) - 4, CY - (int)(sin(m) * lr) - 4,
                    gz_lodge(k), QUAD_C[gz_lodge_quad(k)]);
    }
    for (int q = 0; q < 4; q++)
        lcd_arc(CX, CY, 58, -(q * 90 + 45), -(q * 90 - 45), QUAD_C[q]);
    for (int r = 29; r <= 35; r++)
        lcd_arc(CX, CY, r, -(90 - st.sunCi * 30 + 15), -(90 - st.sunCi * 30 - 15), LC_GOLD);
    for (int i = 0; i < 12; i++) {
        double b = (90 - i * 30 - 15) * D2R;
        for (int r = 22; r <= 42; r++)
            lcd_pixel(CX + (int)(cos(b) * r), CY - (int)(sin(b) * r), LC_LINE);
        double m = (90 - i * 30) * D2R;
        lcd_text16s(CX + (int)(cos(m) * 32) - 8, CY - (int)(sin(m) * 32) - 4,
                    gz_ci(i), i == st.sunCi ? LC_GOLD : LC_MUT);
    }
    double ha = (90 - st.hourZhi * 30) * D2R;
    for (int r = 24; r <= 40; r++)
        lcd_pixel(CX + (int)(cos(ha) * r), CY - (int)(sin(ha) * r), LC_CINN2);
    lcd_circle(CX + (int)(cos(ha) * 40), CY - (int)(sin(ha) * 40), 2, 1, LC_CINN2);
    double ms0, mw;
    gz_lodge_disp(st.midIdx, &ms0, &mw);
    double ma = (ms0 + mw / 2) * D2R;
    for (int r = 42; r <= 60; r++)
        lcd_pixel(CX + (int)(cos(ma) * r), CY - (int)(sin(ma) * r), LC_GOLD2);
    lcd_circle(CX + (int)(cos(ma) * 60), CY - (int)(sin(ma) * 60), 2, 1, LC_GOLD2);
    char b[24];
    snprintf(b, sizeof b, "中星%s", gz_lodge(st.midIdx));
    lcd_text16s(CX - 12, 146, b, LC_SILK);
    snprintf(b, sizeof b, "月将%s", gz_zhi(st.yueJiang));
    lcd_text16s(CX - 12, 162, b, LC_GOLD);
    lcd_text16(112, 82, "北", LC_SILK);
    lcd_text16(112, 222, "南", LC_SILK);
    lcd_text16(30, 152, "西", LC_SILK);
    lcd_text16(194, 152, "东", LC_SILK);
    snprintf(b, sizeof b, "日躔%s", gz_ci(st.sunCi));
    lcd_text16s(14, 82, b, LC_GOLD2);
    char lst[16];
    snprintf(lst, sizeof lst, "LST%5.1f", st.lst);
    lcd_text8(162, 82, lst, LC_MUT, 1);
    snprintf(b, sizeof b, "时辰%s", gz_zhi(st.hourZhi));
    lcd_text16s(14, 222, b, LC_CINN2);
    snprintf(b, sizeof b, "%s%d度", gz_lodge(st.midIdx), gz_lodge_du(st.midIdx));
    lcd_text16s(226 - lcd_text16_width(b) / 2, 222, b, LC_MUT);
    lcd_flush();
}

static void center16(int y, const char *s)
{
    int w = ssd1306_text16_width(s);
    ssd1306_text16((128 - w) / 2, y, s, 1);
}

static void render_splash(void)
{
    if (s_lcd) { render_splash_lcd(); return; }
    ssd1306_clear();
    center16(8, "干支钟");
    ssd1306_text8(16, 32, "GANZHI CLOCK", 1, 1);
    center16(48, "启动中");
    ssd1306_flush();
}

static void render_clock(void)
{
    if (s_lcd) { render_clock_lcd(); return; }
    ssd1306_clear();
    char p[7];
    const int idx[4] = { s_now.yIdx, s_now.mIdx, s_now.dIdx, s_now.hIdx };
    for (int i = 0; i < 4; i++) {
        gz_pillar(idx[i], p);
        ssd1306_text16(i * 32, 0, p, 1);
    }
    char t[12];
    snprintf(t, sizeof t, "%02d:%02d:%02d", s_now.h, s_now.mi, s_now.s);
    ssd1306_text8(0, 16, t, 1, 2);

    char l[40];
    snprintf(l, sizeof l, "%s%s%s", s_now.lunarLeap ? "闰" : "",
             gz_yue(s_now.lunarM), gz_ri(s_now.lunarD));
    ssd1306_text16(0, 48, l, 1);
    char sc[10];
    snprintf(sc, sizeof sc, "%s时", gz_zhi(s_now.hBranch));
    ssd1306_text16(80, 48, sc, 1);
    ssd1306_text16(112, 48, app_synced() ? "校" : "未", 1);
    ssd1306_flush();
}

static void draw_yao(int i, const ly_chart_t *c, int y, int spin_phase)
{
    const ly_line_t *ln = &c->lines[i];
    if (ln->yang) ssd1306_rect(2, y + 2, 36, 3, 1);
    else { ssd1306_rect(2, y + 2, 16, 3, 1); ssd1306_rect(22, y + 2, 16, 3, 1); }

    if (spin_phase >= 0) {
        ssd1306_circle(20, y + 4, 3, 0, 1);
        float a = (float)spin_phase * 0.35f + i;
        ssd1306_pixel(20 + (int)(cosf(a) * 2), y + 4 + (int)(sinf(a) * 2), 1);
        return;
    }
    if (ln->moving) {
        if (ln->yang) ssd1306_circle(43, y + 4, 3, 0, 1);
        else {
            for (int k = 0; k < 7; k++) {
                ssd1306_pixel(40 + k, y + 1 + k, 1);
                ssd1306_pixel(46 - k, y + 1 + k, 1);
            }
        }
    }
    if (i == c->shi) ssd1306_tri(50, y + 1, 7, 7, 1, 1);
    else if (i == c->ying) ssd1306_tri(50, y + 1, 7, 7, 0, 1);
    if (c->kong[0] == c->zhiIdx[i] || c->kong[1] == c->zhiIdx[i])
        ssd1306_circle(63, y + 4, 3, 0, 1);

    ssd1306_text16s(72, y, gz_zhi(c->zhiIdx[i]), 1);
    ssd1306_text16s(82, y, ly_rel(c->rel[i]), 1);
    ssd1306_text16s(100, y, ly_spirit(c->spirit[i]), 1);
}

static void render_liuyao(int64_t now_us)
{
    if (s_lcd) { render_liuyao_lcd(now_us); return; }
    ssd1306_clear();
    const ly_chart_t *c = app_chart();
    uint32_t seq = app_cast_seq();
    if (seq != s_last_seq) { s_last_seq = seq; s_anim_start = now_us; }
    int64_t el = (s_anim_start < 0) ? ANIM_US : now_us - s_anim_start;
    int animating = c && el < ANIM_US;

    if (!c) {
        center16(8, "按键起卦");
        ssd1306_text8(20, 32, "KEY0 = CAST", 1, 1);
        center16(48, "短按起卦");
        ssd1306_flush();
        return;
    }
    if (animating) {
        ssd1306_text16(0, 0, "起卦中", 1);
        char d[8];
        snprintf(d, sizeof d, "%d%%", (int)(el * 100 / ANIM_US));
        ssd1306_text8(104, 0, d, 1, 1);
    } else if (c->hasMoving) {
        ssd1306_text16(0, 0, c->name, 1);
        ssd1306_text16(50, 0, "之", 1);
        ssd1306_text16(68, 0, c->changedName, 1);
    } else {
        char h[40];
        snprintf(h, sizeof h, "%s静卦", c->name);
        center16(0, h);
    }
    for (int i = 0; i < 6; i++) {
        int y = 56 - i * 8;
        int phase = -1;
        if (animating) {
            if (el < (int64_t)(i + 1) * 140000) phase = (int)(el / 50000);
            else phase = -1;
        }
        draw_yao(i, c, y, phase);
    }
    ssd1306_flush();
}

static void render_status(void)
{
    if (s_lcd) { render_status_lcd(); return; }
    ssd1306_clear();
    bool online = app_ip()[0] != 0;
    const char *st = app_mode() == DEV_SETUP ? "热点" : (online ? "已连" : "离线");
    ssd1306_text16(0, 0, "网络", 1);
    ssd1306_text16(32, 0, st, 1);
    ssd1306_text16(64, 0, "校时", 1);
    ssd1306_text16(96, 0, app_synced() ? "已" : "未", 1);
    if (app_ap_on()) ssd1306_text16(112, 0, "热", 1);

    ssd1306_text8(0, 16, "SSID", 1, 1);
    const char *ssid = app_mode() == DEV_SETUP ? AP_SSID : app_ssid();
    if (ssid[0]) {
        char s[12];
        snprintf(s, sizeof s, "%.11s", ssid);
        ssd1306_text8(40, 16, s, 1, 1);
    } else ssd1306_text16(40, 16, "未配置", 1);

    if (app_prov() == PROV_FAIL) {
        char r[26];
        snprintf(r, sizeof r, "%.8s", app_fail_reason());
        ssd1306_text16(0, 32, r, 1);
        ssd1306_text8(0, 48, "HOLD BOOT=RETRY", 1, 1);
    } else if (app_prov() == PROV_CONNECTING) {
        ssd1306_text16(0, 32, "连接中", 1);
        ssd1306_text8(0, 48, "CONNECTING...", 1, 1);
    } else {
        const char *ip = app_ip()[0] ? app_ip() : "0.0.0.0";
        ssd1306_text8(0, 32, ip, 1, 1);
        char u[16];
        snprintf(u, sizeof u, "UP %lus", (unsigned long)app_uptime_s());
        ssd1306_text8(0, 48, u, 1, 1);
        ssd1306_text8(88, 48, APP_VER, 1, 1);
    }
    ssd1306_flush();
}

static void render_star(void)
{
    if (s_lcd) { render_star_lcd(); return; }
    ssd1306_clear();
    gz_star_t st;
    gz_star(s_now.epoch, app_lon_e6() / 1e6, &st);
    char l[40];
    snprintf(l, sizeof l, "中星%s", gz_lodge(st.midIdx));
    ssd1306_text16(0, 0, l, 1);
    snprintf(l, sizeof l, "日躔%s", gz_ci(st.sunCi));
    ssd1306_text16(0, 16, l, 1);
    snprintf(l, sizeof l, "月将%s", gz_zhi(st.yueJiang));
    ssd1306_text16(0, 32, l, 1);
    snprintf(l, sizeof l, "%s时", gz_zhi(st.hourZhi));
    ssd1306_text16(0, 48, l, 1);
    char a[16];
    snprintf(a, sizeof a, "LST%5.1f", st.lst);
    ssd1306_text8(56, 48, a, 1, 1);
    ssd1306_flush();
}

static void ui_task(void *arg)
{
    int64_t boot = esp_timer_get_time();
    s_lcd = ssd1306_is_lcd();
    render_splash();
    gz_compute(app_epoch_ms(), &s_now);   /* 预热节气缓存 */

    int last_sec = -1, last_view = -1, last_th = -1, last_br = -1;
    bool sleep_applied = false, was_sleep = false;
    uint32_t last_seq = 0;

    for (;;) {
        int64_t now_us = esp_timer_get_time();
        bool sl = app_display_sleep();
        if (sl) {
            if (!sleep_applied) { ssd1306_sleep(true); sleep_applied = true; }
            was_sleep = true;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (sleep_applied) { ssd1306_sleep(false); sleep_applied = false; }

        int th = app_theme();
        int br = app_brightness();
        if (th != last_th || br != last_br) {
            if (s_lcd) lcd_palette(br & 3, th);
            else { ssd1306_invert(th == 1 ? 1 : 0); ssd1306_contrast(BRIGHT[br & 3]); }
            last_th = th; last_br = br;
        }

        int view = app_view();
        uint32_t seq = app_cast_seq();
        int64_t epoch = app_epoch_ms();
        int sec = (int)(epoch / 1000);
        bool anim = (view == 1) && (s_anim_start >= 0) && (now_us - s_anim_start < ANIM_US);
        bool need = was_sleep || view != last_view || sec != last_sec || seq != last_seq || anim;
        was_sleep = false;

        if (need) {
            if (now_us - boot < 900000 && view == 0 && seq == 0) {
                render_splash();
            } else {
                if (sec != last_sec || view != last_view) gz_compute(epoch, &s_now);
                if (view == 0) render_clock();
                else if (view == 1) render_liuyao(now_us);
                else if (view == 2) render_status();
                else render_star();
            }
            last_sec = sec; last_view = view; last_seq = seq;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void ui_start(void)
{
    xTaskCreate(ui_task, "ui", 6144, NULL, 5, NULL);
    ESP_LOGI(TAG, "ui task started");
}