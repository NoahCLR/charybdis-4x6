#pragma once

#include "qmk_stub.h"

#ifndef WS2812_LED_COUNT
#    define WS2812_LED_COUNT RGB_MATRIX_LED_COUNT
#endif

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws2812_led_t;

extern ws2812_led_t ws2812_leds[WS2812_LED_COUNT];
