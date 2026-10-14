#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef union {
    uint8_t raw;
    struct { bool num_lock:1; bool caps_lock:1; bool scroll_lock:1; bool compose:1; bool kana:1; };
} led_t;
led_t host_keyboard_led_state(void);
