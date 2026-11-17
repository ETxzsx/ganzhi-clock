#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "st7789.h"
#include "xl9555.h"
#include "gz_font.h"

#define TAG "ssd1306"
#define SDA_IO 5
#define SCL_IO 4
#define DC_IO  38
#define D2_IO  6
#define ADDR   0x3c

static i2c_master_dev_handle_t s_dev;
static uint8_t s_fb[8][128];
static uint8_t s_tx[8 + 1024];
static bool s_ok;
static bool s_lcd;
static bool s_err_logged;

void ssd1306_attach_lcd(void) { s_lcd = true; }
bool ssd1306_is_lcd(void) { return s_lcd; }

static void cmd(uint8_t c)
{
    uint8_t b[2] = { 0x00, c };
    esp_err_t e = i2c_master_transmit(s_dev, b, 2, 100);
    if (e != ESP_OK && !s_err_logged) { s_err_logged = true; ESP_LOGE(TAG, "cmd %02x: %s", c, esp_err_to_name(e)); }
}
static void cmd2(uint8_t c, uint8_t p)
{
    uint8_t b[3] = { 0x00, c, p };
    esp_err_t e = i2c_master_transmit(s_dev, b, 3, 100);
    if (e != ESP_OK && !s_err_logged) { s_err_logged = true; ESP_LOGE(TAG, "cmd %02x: %s", c, esp_err_to_name(e)); }
}

esp_err_t ssd1306_init(void)
{
    gpio_config_t g = {
        .pin_bit_mask = 1ULL << DC_IO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&g);
    gpio_set_level(DC_IO, 0);   /* SA0=0 -> 0x3C */
    gpio_config_t g2 = {
        .pin_bit_mask = 1ULL << D2_IO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&g2);
    xl9555_oled_reset();

    i2c_master_bus_config_t bus = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = SDA_IO,
        .scl_io_num = SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bh;
    esp_err_t e = i2c_new_master_bus(&bus, &bh);
    if (e != ESP_OK) { ESP_LOGE(TAG, "bus: %s", esp_err_to_name(e)); return e; }
    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ADDR,
        .scl_speed_hz = 400000,
    };
    e = i2c_master_bus_add_device(bh, &dc, &s_dev);
    if (e != ESP_OK) { ESP_LOGE(TAG, "dev: %s", esp_err_to_name(e)); return e; }
    esp_err_t p3c = i2c_master_probe(bh, 0x3c, 100);
    esp_err_t p3d = i2c_master_probe(bh, 0x3d, 100);
    ESP_LOGI(TAG, "probe 3C=%s 3D=%s", esp_err_to_name(p3c), esp_err_to_name(p3d));
    if (p3c != ESP_OK && p3d != ESP_OK) {
        ESP_LOGW(TAG, "no OLED ACK on P6 socket (module absent or not in IIC mode)");
        return ESP_ERR_NOT_FOUND;
    }

    static const uint8_t seq[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x30, 0xA4, 0xA6, 0xAF
    };
    for (size_t i = 0; i < sizeof seq; i++) cmd(seq[i]);
    s_ok = true;
    ssd1306_clear();
    ssd1306_flush();
    ESP_LOGI(TAG, "oled ready");
    return ESP_OK;
}

void ssd1306_clear(void) { memset(s_fb, 0, sizeof s_fb); }

void ssd1306_flush(void)
{
    if (s_lcd) return;   /* LCD 模式由 ui 直接驱动 lcd_* 画布 */
    if (!s_ok) return;
    static const uint8_t head[8] = { 0x00, 0x21, 0, 127, 0x22, 0, 7, 0x40 };
    memcpy(s_tx, head, 8);
    memcpy(s_tx + 8, s_fb, 1024);
    esp_err_t e = i2c_master_transmit(s_dev, s_tx, sizeof s_tx, 200);
    if (e != ESP_OK && !s_err_logged) { s_err_logged = true; ESP_LOGE(TAG, "flush: %s", esp_err_to_name(e)); }
}

void ssd1306_pixel(int x, int y, int on)
{
    if (x < 0 || x > 127 || y < 0 || y > 63) return;
    if (on) s_fb[y >> 3][x] |= (uint8_t)(1 << (y & 7));
    else s_fb[y >> 3][x] &= (uint8_t)~(1 << (y & 7));
}

void ssd1306_rect(int x, int y, int w, int h, int on)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            ssd1306_pixel(x + i, y + j, on);
}

void ssd1306_frame(int x, int y, int w, int h, int on)
{
    ssd1306_rect(x, y, w, 1, on);
    ssd1306_rect(x, y + h - 1, w, 1, on);
    ssd1306_rect(x, y, 1, h, on);
    ssd1306_rect(x + w - 1, y, 1, h, on);
}

void ssd1306_circle(int cx, int cy, int r, int filled, int on)
{
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            int hit = filled ? (d2 <= r * r)
                             : (d2 <= r * r && d2 >= (r - 1) * (r - 1));
            if (hit) ssd1306_pixel(cx + dx, cy + dy, on);
        }
    }
}

void ssd1306_tri(int x, int y, int w, int h, int filled, int on)
{
    for (int j = 0; j < h; j++) {
        int span = (h > 1) ? (j * (w - 1)) / (h - 1) : 0;
        int x0 = x + (w - 1 - span) / 2;
        if (filled || j == h - 1) ssd1306_rect(x0, y + j, span + 1, 1, on);
        else { ssd1306_pixel(x0, y + j, on); ssd1306_pixel(x0 + span, y + j, on); }
    }
}

static const gz_glyph16_t *glyph(uint32_t cp)
{
    int lo = 0, hi = GZ_FONT16_N - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (GZ_FONT16[mid].cp == cp) return &GZ_FONT16[mid];
        if (GZ_FONT16[mid].cp < cp) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static void draw16(int x, int y, const uint8_t *b, int on)
{
    for (int c = 0; c < 16; c++) {
        uint16_t col = (uint16_t)(b[c * 2] | (b[c * 2 + 1] << 8));
        for (int r = 0; r < 16; r++)
            if (col & (1 << r)) ssd1306_pixel(x + c, y + r, on);
    }
}
static void draw8(int x, int y, const uint8_t *b, int on, int scale)
{
    for (int c = 0; c < 8; c++) {
        uint16_t col = (uint16_t)(b[c * 2] | (b[c * 2 + 1] << 8));
        for (int r = 0; r < 16; r++)
            if (col & (1 << r)) ssd1306_rect(x + c * scale, y + r * scale, scale, scale, on);
    }
}

static int utf8_next(const char **p)
{
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t cp;
    int n;
    if (s[0] < 0x80) { cp = s[0]; n = 1; }
    else if ((s[0] & 0xE0) == 0xC0) { cp = s[0] & 0x1F; n = 2; }
    else if ((s[0] & 0xF0) == 0xE0) { cp = s[0] & 0x0F; n = 3; }
    else if ((s[0] & 0xF8) == 0xF0) { cp = s[0] & 0x07; n = 4; }
    else { *p += 1; return -1; }
    for (int i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) { *p += 1; return -1; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *p += n;
    return (int)cp;
}

int ssd1306_text16(int x, int y, const char *utf8, int on)
{
    int x0 = x;
    const char *p = utf8;
    while (*p) {
        int cp = utf8_next(&p);
        if (cp < 0) continue;
        if (cp < 0x80) {
            if (cp >= 32 && cp <= 126) draw8(x, y, GZ_ASC8[cp - 32], on, 1);
            x += 8;
        } else {
            const gz_glyph16_t *g = glyph((uint32_t)cp);
            if (g) draw16(x, y, g->b, on);
            x += 16;
        }
    }
    return x - x0;
}

int ssd1306_text16_width(const char *utf8)
{
    int w = 0;
    const char *p = utf8;
    while (*p) {
        int cp = utf8_next(&p);
        if (cp < 0) continue;
        w += (cp < 0x80) ? 8 : 16;
    }
    return w;
}

int ssd1306_text16s(int x, int y, const char *utf8, int on)
{
    int x0 = x;
    const char *p = utf8;
    while (*p) {
        int cp = utf8_next(&p);
        if (cp < 0) continue;
        if (cp < 0x80) {
            if (cp >= 32 && cp <= 126) {
                const uint8_t *b = GZ_ASC8[cp - 32];
                for (int c = 0; c < 8; c += 2) {
                    uint16_t col = (uint16_t)(b[c * 2] | (b[c * 2 + 1] << 8));
                    for (int r = 0; r < 16; r += 2)
                        if (col & (1 << r)) ssd1306_pixel(x + c / 2, y + r / 2, on);
                }
            }
            x += 4;
        } else {
            const gz_glyph16_t *g = glyph((uint32_t)cp);
            if (g) {
                for (int c = 0; c < 16; c += 2) {
                    uint16_t col = (uint16_t)(g->b[c * 2] | (g->b[c * 2 + 1] << 8));
                    for (int r = 0; r < 16; r += 2)
                        if (col & (1 << r)) ssd1306_pixel(x + c / 2, y + r / 2, on);
                }
            }
            x += 8;
        }
    }
    return x - x0;
}

int ssd1306_text8(int x, int y, const char *ascii, int on, int scale)
{
    int x0 = x;
    for (const char *p = ascii; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 32 || c > 126) c = '?';
        draw8(x, y, GZ_ASC8[c - 32], on, scale);
        x += 8 * scale;
    }
    return x - x0;
}

void ssd1306_contrast(uint8_t v)
{
    if (s_lcd) return;
    if (s_ok) cmd2(0x81, v);
}
void ssd1306_invert(bool on)
{
    if (s_lcd) return;
    if (s_ok) cmd(on ? 0xA7 : 0xA6);
}
void ssd1306_sleep(bool on)
{
    if (s_lcd) { st7789_sleep(on); return; }
    if (s_ok) cmd(on ? 0xAE : 0xAF);
}