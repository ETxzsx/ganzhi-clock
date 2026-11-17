#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "st7789.h"
#include "xl9555.h"
#include "gz_font.h"

#define TAG "st7789"
#define MOSI_IO 11
#define SCLK_IO 12
#define MISO_IO 13
#define CS_IO   21
#define DC_IO   40

#define LCD_W 240
#define LCD_H 320
#define FB_BYTES (LCD_W * LCD_H * 2)

/* 玄夜鎏金（与 www 暗色主题同源）/ 反色=旧纸浅色主题 */
static const uint16_t PAL_DARK[LC_N] = {
    0x0862, 0x10C5, 0x2189, 0xEF39, 0x9516,
    0xDD6A, 0xF692, 0xE2A7, 0xFC0C,
    0x55D1, 0x6D1C, 0xCCCB, 0xA3DB, 0xCE9B
};
static const uint16_t PAL_LIGHT[LC_N] = {
    0xE6D7, 0xEF39, 0xC5B1, 0x2923, 0x6B0A,
    0x9381, 0xBC85, 0xB1C4, 0xD2C8,
    0x3389, 0x2AF1, 0x9325, 0x7A75, 0x5B2E
};
/* 素：纯黑白灰阶（与 www mono 主题同源） */
static const uint16_t PAL_MONO[LC_N] = {
    0x0000, 0x1082, 0x39C7, 0xFFFF, 0x7BCF,
    0xEF5D, 0xFFFF, 0xDEDB, 0xFFFF,
    0xCE79, 0xBDF7, 0xC638, 0xB5B6, 0xE71C
};

static spi_device_handle_t s_dev;
static bool s_ok;
static uint16_t *s_fb;
static uint16_t s_pal[LC_N];

static void dc(int v) { gpio_set_level(DC_IO, v); }

static void wcmd(uint8_t c)
{
    spi_transaction_t t = { .length = 8, .tx_buffer = &c };
    dc(0);
    spi_device_polling_transmit(s_dev, &t);
}
static void wdata(const void *d, int len)
{
    if (!len) return;
    spi_transaction_t t = { .length = len * 8, .tx_buffer = d };
    dc(1);
    spi_device_polling_transmit(s_dev, &t);
}
static void wcmd1(uint8_t c, uint8_t p) { wcmd(c); wdata(&p, 1); }

static void window(int x0, int y0, int x1, int y1)
{
    uint8_t b[4] = { (uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1 };
    wcmd(0x2A); wdata(b, 4);
    b[0] = (uint8_t)(y0 >> 8); b[1] = (uint8_t)y0; b[2] = (uint8_t)(y1 >> 8); b[3] = (uint8_t)y1;
    wcmd(0x2B); wdata(b, 4);
    wcmd(0x2C);
}

static uint16_t mix(uint16_t a, uint16_t b, int t)
{
    int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
    int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    int r = ar + (br - ar) * t / 256, g = ag + (bg - ag) * t / 256, bl = ab + (bb - ab) * t / 256;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

/* 慢速读寄存器：ST7789 读周期 >=150ns，用 1MHz 设备读 ID/电源模式（仅作诊断） */
static void rreg(uint8_t cmd, int n, uint8_t *out)
{
    dc(0);
    spi_transaction_t c = { .length = 8, .tx_buffer = &cmd };
    spi_device_polling_transmit(s_dev, &c);
    dc(1);
    spi_transaction_t r = {
        .length = 0,
        .rxlength = n * 8,
        .flags = SPI_TRANS_USE_RXDATA,
    };
    esp_err_t e = spi_device_polling_transmit(s_dev, &r);
    if (e != ESP_OK) { ESP_LOGE(TAG, "read %02x: %s", cmd, esp_err_to_name(e)); return; }
    memcpy(out, r.rx_data, n);
}

void lcd_palette(int level, int theme)
{
    const uint16_t *t = theme == 2 ? PAL_MONO : theme == 1 ? PAL_LIGHT : PAL_DARK;
    int dim = (3 - (level & 3)) * 50;
    for (int i = 0; i < LC_N; i++) s_pal[i] = mix(t[i], t[LC_BG], dim);
}

void lcd_clear(int c)
{
    if (!s_fb) return;
    uint16_t v = s_pal[c];
    for (int i = 0; i < LCD_W * LCD_H; i++) s_fb[i] = v;
}

void lcd_pixel(int x, int y, int c)
{
    if (!s_fb || x < 0 || x >= LCD_W || y < 0 || y >= LCD_H) return;
    s_fb[y * LCD_W + x] = s_pal[c];
}

void lcd_rect(int x, int y, int w, int h, int c)
{
    if (!s_fb) return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > LCD_W ? LCD_W : x + w, y1 = y + h > LCD_H ? LCD_H : y + h;
    uint16_t v = s_pal[c];
    for (int j = y0; j < y1; j++) {
        uint16_t *row = s_fb + j * LCD_W;
        for (int i = x0; i < x1; i++) row[i] = v;
    }
}

void lcd_frame(int x, int y, int w, int h, int c)
{
    lcd_rect(x, y, w, 1, c);
    lcd_rect(x, y + h - 1, w, 1, c);
    lcd_rect(x, y, 1, h, c);
    lcd_rect(x + w - 1, y, 1, h, c);
}

void lcd_circle(int cx, int cy, int r, int filled, int c)
{
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            int hit = filled ? (d2 <= r * r) : (d2 <= r * r && d2 >= (r - 1) * (r - 1));
            if (hit) lcd_pixel(cx + dx, cy + dy, c);
        }
}

void lcd_arc(int cx, int cy, int r, int a0, int a1, int c)
{
    for (int a = a0; a <= a1; a += 2) {
        float rad = (float)a * 3.14159265f / 180.f;
        lcd_pixel(cx + (int)(cosf(rad) * r), cy + (int)(sinf(rad) * r), c);
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

static void draw16(int x, int y, const uint8_t *b, int c)
{
    for (int col = 0; col < 16; col++) {
        uint16_t m = (uint16_t)(b[col * 2] | (b[col * 2 + 1] << 8));
        for (int r = 0; r < 16; r++)
            if (m & (1 << r)) lcd_pixel(x + col, y + r, c);
    }
}
static void draw8(int x, int y, const uint8_t *b, int c, int scale)
{
    for (int col = 0; col < 8; col++) {
        uint16_t m = (uint16_t)(b[col * 2] | (b[col * 2 + 1] << 8));
        for (int r = 0; r < 16; r++)
            if (m & (1 << r)) lcd_rect(x + col * scale, y + r * scale, scale, scale, c);
    }
}

int lcd_text16(int x, int y, const char *utf8, int c)
{
    int x0 = x;
    const char *p = utf8;
    while (*p) {
        int cp = utf8_next(&p);
        if (cp < 0) continue;
        if (cp < 0x80) {
            if (cp >= 32 && cp <= 126) draw8(x, y, GZ_ASC8[cp - 32], c, 1);
            x += 8;
        } else {
            const gz_glyph16_t *g = glyph((uint32_t)cp);
            if (g) draw16(x, y, g->b, c);
            x += 16;
        }
    }
    return x - x0;
}

int lcd_text16_width(const char *utf8)
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

int lcd_text16s(int x, int y, const char *utf8, int c)
{
    int x0 = x;
    const char *p = utf8;
    while (*p) {
        int cp = utf8_next(&p);
        if (cp < 0) continue;
        if (cp < 0x80) {
            if (cp >= 32 && cp <= 126) {
                const uint8_t *b = GZ_ASC8[cp - 32];
                for (int col = 0; col < 8; col += 2) {
                    uint16_t m = (uint16_t)(b[col * 2] | (b[col * 2 + 1] << 8));
                    for (int r = 0; r < 16; r += 2)
                        if (m & (1 << r)) lcd_pixel(x + col / 2, y + r / 2, c);
                }
            }
            x += 4;
        } else {
            const gz_glyph16_t *g = glyph((uint32_t)cp);
            if (g) {
                for (int col = 0; col < 16; col += 2) {
                    uint16_t m = (uint16_t)(g->b[col * 2] | (g->b[col * 2 + 1] << 8));
                    for (int r = 0; r < 16; r += 2)
                        if (m & (1 << r)) lcd_pixel(x + col / 2, y + r / 2, c);
                }
            }
            x += 8;
        }
    }
    return x - x0;
}

int lcd_text8(int x, int y, const char *ascii, int c, int scale)
{
    int x0 = x;
    for (const char *p = ascii; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch < 32 || ch > 126) ch = '?';
        draw8(x, y, GZ_ASC8[ch - 32], c, scale);
        x += 8 * scale;
    }
    return x - x0;
}

static void seal_glyph(int x, int y, uint32_t cp, int c)
{
    const gz_glyph16_t *g = glyph(cp);
    if (!g) return;
    for (int col = 0; col < 16; col++) {
        uint16_t m = (uint16_t)(g->b[col * 2] | (g->b[col * 2 + 1] << 8));
        for (int r = 0; r < 16; r++)
            if (m & (1 << r)) lcd_rect(x + col * 2, y + r * 2, 2, 2, c);
    }
}

static void seal(int x, int y)
{
    lcd_rect(x, y, 128, 48, LC_CINN);
    lcd_frame(x + 3, y + 3, 122, 42, LC_CINN2);
    seal_glyph(x + 8, y + 8, 0x5E72, LC_SILK);    /* 干 */
    seal_glyph(x + 48, y + 8, 0x652F, LC_SILK);   /* 支 */
    seal_glyph(x + 88, y + 8, 0x949F, LC_SILK);   /* 钟 */
}

static const uint16_t STARS[][2] = {
    {12, 8}, {40, 22}, {76, 6}, {110, 30}, {150, 12}, {188, 26}, {222, 8},
    {20, 44}, {58, 58}, {96, 40}, {134, 62}, {172, 48}, {210, 60}, {232, 36},
    {8, 68}, {48, 74}, {128, 70}, {196, 72}, {228, 66},
    {14, 250}, {52, 262}, {92, 246}, {130, 268}, {170, 252}, {208, 266}, {230, 248},
    {24, 288}, {64, 300}, {104, 284}, {144, 306}, {184, 290}, {220, 302},
    {10, 314}, {80, 316}, {160, 312}, {236, 314},
};

void lcd_decor(void)
{
    for (size_t i = 0; i < sizeof STARS / sizeof STARS[0]; i++)
        lcd_pixel(STARS[i][0], STARS[i][1], (i % 7 == 0) ? LC_GOLD : LC_LINE);
    lcd_rect(10, 80, 1, 160, LC_LINE);
    lcd_rect(229, 80, 1, 160, LC_LINE);
    lcd_rect(10, 80, 7, 1, LC_GOLD);
    lcd_rect(223, 80, 7, 1, LC_GOLD);
    lcd_rect(10, 239, 7, 1, LC_GOLD);
    lcd_rect(223, 239, 7, 1, LC_GOLD);
    seal(64, 16);
    seal(64, 256);
}

void lcd_flush(void)
{
    if (!s_ok || !s_fb) return;
    window(0, 0, LCD_W - 1, LCD_H - 1);
    dc(1);
    /* S3 DMA 单笔上限 32KB：按 64 行分块，同一 GRAM 写窗口内续写 */
    const int CHUNK = 64 * LCD_W * 2;
    for (int off = 0; off < FB_BYTES; off += CHUNK) {
        int len = FB_BYTES - off;
        if (len > CHUNK) len = CHUNK;
        spi_transaction_t t = { .length = len * 8, .tx_buffer = (const uint8_t *)s_fb + off };
        esp_err_t e = spi_device_polling_transmit(s_dev, &t);
        if (e != ESP_OK) { ESP_LOGE(TAG, "flush: %s", esp_err_to_name(e)); return; }
    }
}

esp_err_t st7789_init(void)
{
    gpio_config_t g = { .pin_bit_mask = 1ULL << DC_IO, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&g);
    dc(1);

    s_fb = heap_caps_malloc(FB_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if (s_fb) ESP_LOGI(TAG, "fb %dB in PSRAM", FB_BYTES);
    else {
        s_fb = heap_caps_malloc(FB_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        ESP_LOGW(TAG, "fb %dB fallback internal: %p", FB_BYTES, (void *)s_fb);
    }
    if (!s_fb) return ESP_ERR_NO_MEM;

    spi_bus_config_t bus = {
        .mosi_io_num = MOSI_IO,
        .miso_io_num = MISO_IO,
        .sclk_io_num = SCLK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = FB_BYTES,
    };
    esp_err_t e = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (e != ESP_OK) { ESP_LOGE(TAG, "bus: %s", esp_err_to_name(e)); return e; }

    xl9555_lcd_power(false);
    xl9555_lcd_reset();

    spi_device_interface_config_t slow = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = CS_IO,
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    e = spi_bus_add_device(SPI2_HOST, &slow, &s_dev);
    if (e != ESP_OK) { ESP_LOGE(TAG, "dev: %s", esp_err_to_name(e)); return e; }

    /* 模块排针通常不接 MISO，读 ID 仅作诊断日志 */
    uint8_t id[4] = { 0 }, pm[2] = { 0 };
    rreg(0x04, 4, id);
    rreg(0x0A, 2, pm);
    ESP_LOGI(TAG, "RDDID=%02x %02x %02x %02x RDDPM=%02x %02x", id[0], id[1], id[2], id[3], pm[0], pm[1]);
    spi_bus_remove_device(s_dev);
    s_dev = NULL;

    spi_device_interface_config_t fast = {
        .clock_speed_hz = 40000000,
        .mode = 0,
        .spics_io_num = CS_IO,
        .queue_size = 1,
    };
    e = spi_bus_add_device(SPI2_HOST, &fast, &s_dev);
    if (e != ESP_OK) { ESP_LOGE(TAG, "dev: %s", esp_err_to_name(e)); return e; }

    wcmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    wcmd1(0x36, 0x00);                 /* 竖屏，列=玻璃满宽，行窗口兼容两种批次 */
    wcmd1(0x3A, 0x55);                 /* RGB565 */
    static const uint8_t b2[] = { 0x0C, 0x0C, 0x00, 0x33, 0x33 };
    wcmd(0xB2); wdata(b2, sizeof b2);
    wcmd1(0xB7, 0x75);
    wcmd1(0xBB, 0x1C);
    wcmd1(0xC0, 0x2C);
    wcmd1(0xC2, 0x01);
    wcmd1(0xC3, 0x0F);
    wcmd1(0xC4, 0x20);
    wcmd1(0xC6, 0x01);
    static const uint8_t d0[] = { 0xA4, 0xA1 };
    wcmd(0xD0); wdata(d0, sizeof d0);
    static const uint8_t e0[] = { 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23 };
    static const uint8_t e1[] = { 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23 };
    wcmd(0xE0); wdata(e0, sizeof e0);
    wcmd(0xE1); wdata(e1, sizeof e1);
    wcmd(0x21);
    wcmd(0x13);
    wcmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));

    s_ok = true;
    xl9555_lcd_power(true);
    lcd_palette(3, 0);
    lcd_clear(LC_BG);
    lcd_decor();
    lcd_flush();
    ESP_LOGI(TAG, "lcd ready 240x320 native color, band y80..239");
    return ESP_OK;
}

void st7789_sleep(bool on) { if (s_ok) xl9555_lcd_power(!on); }