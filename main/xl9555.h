#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* XL9555 IO 扩展 (I2C0: SDA=IO41 SCL=IO42, 地址 0x20)
   P0.5 = OLED 复位, P1.2 = LCD 复位, P1.3 = LCD 背光, P1.4..P1.7 = KEY3..KEY0 (按下为低) */
esp_err_t xl9555_init(void);
void xl9555_oled_reset(void);
void xl9555_lcd_reset(void);
void xl9555_lcd_power(bool on);
uint8_t xl9555_keys(void);   /* bit0=KEY0 bit1=KEY1 bit2=KEY2 bit3=KEY3, 1=按下 */
void xl9555_led(bool on);