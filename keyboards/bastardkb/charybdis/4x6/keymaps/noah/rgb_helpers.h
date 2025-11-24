#pragma once

#ifdef RGB_MATRIX_ENABLE

#    include "rgb_matrix.h"

// ------------------------------------------------------------
// Core helpers: always pass led_min / led_max from
// rgb_matrix_indicators_advanced_user()
// ------------------------------------------------------------

static inline void set_led_rgb(uint8_t index, uint8_t led_min, uint8_t led_max, uint8_t r, uint8_t g, uint8_t b) {
    if (index < led_min || index >= led_max) {
        return;
    }
    rgb_matrix_set_color(index, r, g, b);
}

static inline void set_led_color(uint8_t index, uint8_t led_min, uint8_t led_max, rgb_t color) {
    set_led_rgb(index, led_min, led_max, color.r, color.g, color.b);
}

static inline void set_led_group(const uint8_t *indices, uint8_t count, uint8_t led_min, uint8_t led_max, rgb_t color) {
    for (uint8_t i = 0; i < count; i++) {
        set_led_color(indices[i], led_min, led_max, color);
    }
}

static inline void fill_led_range(uint8_t from, uint8_t to, uint8_t led_min, uint8_t led_max, rgb_t color) {
    if (from >= to) return;

    if (from < led_min) from = led_min;
    if (to > led_max) to = led_max;

    for (uint8_t i = from; i < to; i++) {
        set_led_color(i, led_min, led_max, color);
    }
}

// ------------------------------------------------------------
// Convenience halves helpers
// ------------------------------------------------------------

// "Left side" = entire physical left half
// -> only the half with led_min == 0 does any work
static inline void set_left_side(rgb_t color, uint8_t led_min, uint8_t led_max) {
    if (led_min != 0) {
        return; // right half: do nothing
    }
    fill_led_range(led_min, led_max, led_min, led_max, color);
}

// "Right side" = entire physical right half
// -> only the half with led_min > 0 does any work
static inline void set_right_side(rgb_t color, uint8_t led_min, uint8_t led_max) {
    if (led_min == 0) {
        return; // left half: do nothing
    }
    fill_led_range(led_min, led_max, led_min, led_max, color);
}

// "Both sides" = all LEDs on this half
static inline void set_both_sides(rgb_t color, uint8_t led_min, uint8_t led_max) {
    fill_led_range(led_min, led_max, led_min, led_max, color);
}

// Clamp HSV value to a provided maximum value.
static inline hsv_t clamp_hsv_value(hsv_t hsv, uint8_t max_value) {
    if (hsv.v > max_value) {
        hsv.v = max_value;
    }
    return hsv;
}

#endif // RGB_MATRIX_ENABLE
