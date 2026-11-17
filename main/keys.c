#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "keys.h"
#include "xl9555.h"

#define TAG "keys"
#define BOOT_GPIO GPIO_NUM_0
#define TICK_MS 20

static key_cb_t s_cb;
static const int s_long_ms[KEY_COUNT] = { 3000, 0, 0, 0, 2000 };
static const int s_short_max[KEY_COUNT] = { 1500, 2500, 2500, 2500, 1500 };

static void keys_task(void *arg)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    int last_raw[KEY_COUNT] = { 0 };
    int deb[KEY_COUNT] = { 0 };
    int state[KEY_COUNT] = { 0 };
    int held[KEY_COUNT] = { 0 };
    int long_done[KEY_COUNT] = { 0 };

    for (;;) {
        int mask = (gpio_get_level(BOOT_GPIO) ? 0 : 1) | (xl9555_keys() << 1);
        for (int k = 0; k < KEY_COUNT; k++) {
            int bit = (mask >> k) & 1;
            if (bit != last_raw[k]) { last_raw[k] = bit; deb[k] = 0; }
            else if (deb[k] < 2) deb[k]++;
            if (deb[k] >= 2 && bit != state[k]) {
                state[k] = bit;
                if (bit) { held[k] = 0; long_done[k] = 0; }
                else {
                    if (!long_done[k] && held[k] >= 60 && held[k] <= s_short_max[k])
                        s_cb(k, KEV_SHORT);
                    held[k] = 0;
                }
            }
            if (state[k]) {
                held[k] += TICK_MS;
                if (s_long_ms[k] && !long_done[k] && held[k] >= s_long_ms[k]) {
                    long_done[k] = 1;
                    s_cb(k, KEV_LONG);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

void keys_start(key_cb_t cb)
{
    s_cb = cb;
    xTaskCreate(keys_task, "keys", 3072, NULL, 4, NULL);
    ESP_LOGI(TAG, "scan started");
}