#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "xl9555.h"

#define TAG "xl9555"
#define SDA_IO 41
#define SCL_IO 42
#define ADDR   0x20
#define REG_IN0   0
#define REG_IN1   1
#define REG_OUT0  2
#define REG_OUT1  3
#define REG_CFG0  6
#define REG_CFG1  7
#define OV_RESET  0x20
#define GBC_LED   0x40
#define LCD_RST   0x04   /* P1.2 */
#define LCD_PWR   0x08   /* P1.3 */

static i2c_master_dev_handle_t s_dev;
static uint8_t s_out0;
static uint8_t s_out1;

static esp_err_t wr(uint8_t reg, uint8_t val)
{
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_dev, b, 2, 100);
}
static esp_err_t rd(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, 100);
}

esp_err_t xl9555_init(void)
{
    i2c_master_bus_config_t bus = {
        .i2c_port = I2C_NUM_0,
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
    e = i2c_master_probe(bh, ADDR, 100);
    ESP_LOGI(TAG, "probe 0x20: %s", esp_err_to_name(e));
    if (e != ESP_OK) return e;

    uint8_t cfg0 = 0xff, cfg1 = 0xff, in0 = 0, out1 = 0;
    esp_err_t r0 = rd(REG_CFG0, &cfg0);
    esp_err_t r1 = rd(REG_CFG1, &cfg1);
    esp_err_t r2 = rd(REG_IN0, &in0);
    esp_err_t r3 = rd(REG_OUT1, &out1);
    if (r0 != ESP_OK || r1 != ESP_OK || r2 != ESP_OK || r3 != ESP_OK)
        ESP_LOGE(TAG, "reg read: %s %s %s %s", esp_err_to_name(r0), esp_err_to_name(r1), esp_err_to_name(r2), esp_err_to_name(r3));
    s_out0 = in0 | OV_RESET;          /* 复位脚先置高，其余保持原电平 */
    wr(REG_OUT0, s_out0);
    wr(REG_CFG0, cfg0 & (uint8_t)~OV_RESET);  /* P0.5 输出 */
    s_out1 = (out1 | LCD_RST) & (uint8_t)~LCD_PWR;   /* LCD 复位高、背光先关 */
    wr(REG_OUT1, s_out1);
    wr(REG_CFG1, (cfg1 | 0xf0) & (uint8_t)~0x0c);    /* P1.4..7 输入(按键), P1.2/3 输出 */
    ESP_LOGI(TAG, "ready cfg0=%02x cfg1=%02x", cfg0 & ~OV_RESET, (cfg1 | 0xf0) & ~0x0c);
    return ESP_OK;
}

void xl9555_oled_reset(void)
{
    wr(REG_OUT0, s_out0 & (uint8_t)~OV_RESET);
    vTaskDelay(pdMS_TO_TICKS(30));
    s_out0 |= OV_RESET;
    wr(REG_OUT0, s_out0);
    vTaskDelay(pdMS_TO_TICKS(60));
}

void xl9555_lcd_reset(void)
{
    wr(REG_OUT1, s_out1 & (uint8_t)~LCD_RST);
    vTaskDelay(pdMS_TO_TICKS(30));
    s_out1 |= LCD_RST;
    wr(REG_OUT1, s_out1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

void xl9555_lcd_power(bool on)
{
    if (on) s_out1 |= LCD_PWR; else s_out1 &= (uint8_t)~LCD_PWR;
    wr(REG_OUT1, s_out1);
}

uint8_t xl9555_keys(void)
{
    uint8_t v = 0xff;
    if (rd(REG_IN1, &v) != ESP_OK) return 0;
    return (uint8_t)((~v >> 4) & 0x0f);
}

void xl9555_led(bool on)
{
    if (on) s_out0 |= GBC_LED; else s_out0 &= (uint8_t)~GBC_LED;
    wr(REG_OUT0, s_out0);
}