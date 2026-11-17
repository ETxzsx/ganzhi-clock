#pragma once
#include <stdint.h>

/* 板上六键：RST(复位,不可读) + BOOT + KEY0..KEY3 */
enum { KEY_BOOT = 0, KEY_0, KEY_1, KEY_2, KEY_3, KEY_COUNT };
enum { KEV_SHORT = 0, KEV_LONG };

typedef void (*key_cb_t)(int key, int ev);
void keys_start(key_cb_t cb);