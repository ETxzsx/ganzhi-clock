#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* SSD1306 128x64 OLED (I2C1: SCL=IO4 SDA=IO5, 地址 0x3C, DC/SA0=IO38 拉低) */
esp_err_t ssd1306_init(void);
/* OLED 缺席时改由 ST7789 SPI LCD 原生彩色画布输出（ui 走 lcd_* 分支） */
void ssd1306_attach_lcd(void);
bool ssd1306_is_lcd(void);
void ssd1306_clear(void);
void ssd1306_flush(void);
void ssd1306_pixel(int x, int y, int on);
void ssd1306_rect(int x, int y, int w, int h, int on);
void ssd1306_frame(int x, int y, int w, int h, int on);
void ssd1306_circle(int cx, int cy, int r, int filled, int on);
void ssd1306_tri(int x, int y, int w, int h, int filled, int on);
/* 16x16 中文(UTF-8)，返回绘制宽度；缺字以空白占位 */
int ssd1306_text16(int x, int y, const char *utf8, int on);
int ssd1306_text16_width(const char *utf8);
/* 8x8 半采样中文，用于 8px 行高 */
int ssd1306_text16s(int x, int y, const char *utf8, int on);
/* 8x16 ASCII，scale=1/2 */
int ssd1306_text8(int x, int y, const char *ascii, int on, int scale);
void ssd1306_contrast(uint8_t v);
void ssd1306_invert(bool on);
void ssd1306_sleep(bool on);