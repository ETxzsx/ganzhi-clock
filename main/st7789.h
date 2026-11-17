#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ST7789V SPI LCD (WIRELESS 座: MOSI=IO11 SCK=IO12 MISO=IO13 CS=IO21 DC=IO40,
   RST=XL9555 P1.2, PWR/背光=XL9555 P1.3)。兼容 1.3 寸 240x240 与 2.4 寸 320x240：
   竖屏 240x320 原生彩色画布，主内容带 y80..239 落在两种玻璃行窗口交集内。 */
esp_err_t st7789_init(void);
void st7789_sleep(bool on);

/* 原生 RGB565 画布（调色板索引取色，lcd_palette 切换亮度/反色主题） */
enum {
    LC_BG, LC_CARD, LC_LINE, LC_SILK, LC_MUT,
    LC_GOLD, LC_GOLD2, LC_CINN, LC_CINN2,
    LC_JADE, LC_INDIGO, LC_EARTH, LC_VIOLET, LC_SILVER,
    LC_N
};
void lcd_palette(int level, int theme);   /* level 0..3; theme 0夜 1昼 2素(纯黑白) */
void lcd_clear(int c);
void lcd_pixel(int x, int y, int c);
void lcd_rect(int x, int y, int w, int h, int c);
void lcd_frame(int x, int y, int w, int h, int c);
void lcd_circle(int cx, int cy, int r, int filled, int c);
void lcd_arc(int cx, int cy, int r, int a0, int a1, int c);
int lcd_text16(int x, int y, const char *utf8, int c);
int lcd_text16_width(const char *utf8);
int lcd_text16s(int x, int y, const char *utf8, int c);
int lcd_text8(int x, int y, const char *ascii, int c, int scale);
void lcd_decor(void);   /* 上下干支钟印章 + 星点 + 侧栏，随主题取色 */
void lcd_flush(void);