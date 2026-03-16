// ────────────────────────────────────────────────────────────────────────────
// RGB Matrix Helpers (Split-Safe)
// ────────────────────────────────────────────────────────────────────────────
//
// Thin wrappers around QMK's rgb_matrix_set_color() that handle the split
// keyboard's LED addressing.
//
// On a split keyboard, rgb_matrix_indicators_advanced_user() is called in
// "chunks" — led_min to led_max define which LEDs this call should touch.
// Each half only processes its own chunk.  These helpers automatically
// clamp to the current chunk, so callers can use global LED indices (0–57)
// without worrying about which half they're on.
//
// All helpers must be called from inside
// rgb_matrix_indicators_advanced_user(led_min, led_max).
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#if defined(RGB_MATRIX_ENABLE)

#    include "rgb_matrix.h"

// ─── Core helpers ───────────────────────────────────────────────────────────

// Set a single LED by global index.  No-op if the LED is outside this chunk.
static inline void rgb_set_led(uint8_t index, uint8_t led_min, uint8_t led_max, uint8_t r, uint8_t g, uint8_t b) {
    if (index < led_min || index >= led_max) {
        return;
    }
    rgb_matrix_set_color(index, r, g, b);
}

// Same as rgb_set_led but takes an rgb_t struct instead of separate R/G/B.
static inline void rgb_set_led_color(uint8_t index, uint8_t led_min, uint8_t led_max, rgb_t color) {
    rgb_set_led(index, led_min, led_max, color.r, color.g, color.b);
}

// Color a list of non-contiguous LED indices (e.g. specific modifier keys).
static inline void rgb_set_led_group(const uint8_t *indices, uint8_t count, uint8_t led_min, uint8_t led_max, rgb_t color) {
    for (uint8_t i = 0; i < count; i++) {
        rgb_set_led_color(indices[i], led_min, led_max, color);
    }
}

// Color a continuous range of LEDs [from, to), clamped to the current chunk.
static inline void rgb_fill_led_range(uint8_t from, uint8_t to, uint8_t led_min, uint8_t led_max, rgb_t color) {
    if (from >= to) return;

    // Clamp to the current chunk boundaries.
    if (from < led_min) from = led_min;
    if (to > led_max) to = led_max;

    for (uint8_t i = from; i < to; i++) {
        rgb_matrix_set_color(i, color.r, color.g, color.b);
    }
}

// ─── Convenience: target physical halves ────────────────────────────────────
//
// The split boundary is fixed at LED index 29 (matching RGB_MATRIX_SPLIT).
// These helpers let you paint an entire half with one call.

#    ifndef RGB_LEFT_LED_COUNT
#        define RGB_LEFT_LED_COUNT 29 // must match first element of RGB_MATRIX_SPLIT
#    endif

// Color only the left half (LEDs 0–28).
static inline void rgb_set_left_half(rgb_t color, uint8_t led_min, uint8_t led_max) {
    rgb_fill_led_range(0, RGB_LEFT_LED_COUNT, led_min, led_max, color);
}

// Color only the right half (LEDs 29–57).
static inline void rgb_set_right_half(rgb_t color, uint8_t led_min, uint8_t led_max) {
    rgb_fill_led_range(RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, led_min, led_max, color);
}

// Color all LEDs on both halves (clamped to the current chunk per call).
static inline void rgb_set_both_halves(rgb_t color, uint8_t led_min, uint8_t led_max) {
    rgb_fill_led_range(0, RGB_MATRIX_LED_COUNT, led_min, led_max, color);
}

// Cap the value (brightness) channel of an HSV color.
static inline hsv_t clamp_hsv_value(hsv_t hsv, uint8_t max_value) {
    if (hsv.v > max_value) {
        hsv.v = max_value;
    }
    return hsv;
}

#else  // RGB_MATRIX_ENABLE not defined: provide empty stubs so files compile.
static inline void  rgb_set_led(uint8_t index, uint8_t led_min, uint8_t led_max, uint8_t r, uint8_t g, uint8_t b) {}
static inline void  rgb_set_led_color(uint8_t index, uint8_t led_min, uint8_t led_max, rgb_t color) {}
static inline void  rgb_set_led_group(const uint8_t *indices, uint8_t count, uint8_t led_min, uint8_t led_max, rgb_t color) {}
static inline void  rgb_fill_led_range(uint8_t from, uint8_t to, uint8_t led_min, uint8_t led_max, rgb_t color) {}
static inline void  rgb_set_left_half(rgb_t color, uint8_t led_min, uint8_t led_max) {}
static inline void  rgb_set_right_half(rgb_t color, uint8_t led_min, uint8_t led_max) {}
static inline void  rgb_set_both_halves(rgb_t color, uint8_t led_min, uint8_t led_max) {}
static inline hsv_t clamp_hsv_value(hsv_t hsv, uint8_t max_value) {
    if (hsv.v > max_value) {
        hsv.v = max_value;
    }
    return hsv;
}
#endif // RGB_MATRIX_ENABLE
