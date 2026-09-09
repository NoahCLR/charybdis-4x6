#pragma once
#include <stdint.h>
#define RGB_MATRIX_LED_COUNT 3
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 180
typedef struct {uint8_t flags[RGB_MATRIX_LED_COUNT];} led_config_t;
extern led_config_t g_led_config;
enum {
    RGB_MATRIX_NONE,
#define RGB_MATRIX_EFFECT(name, ...) RGB_MATRIX_##name,
#include "rgb_matrix_effects.inc"
#undef RGB_MATRIX_EFFECT
    RGB_MATRIX_EFFECT_MAX
};
void rgb_matrix_enable_noeeprom(void);
void rgb_matrix_enable(void);
void rgb_matrix_disable(void);
void rgb_matrix_mode(uint8_t mode);
void rgb_matrix_set_speed(uint8_t speed);
void rgb_matrix_set_flags(uint8_t flags);
void rgb_matrix_sethsv(uint16_t hue, uint8_t saturation, uint8_t brightness);
