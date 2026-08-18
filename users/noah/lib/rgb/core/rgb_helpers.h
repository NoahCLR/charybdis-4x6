// ────────────────────────────────────────────────────────────────────────────
// RGB Split-Safe Helpers
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

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../key/runtime/feedback_kind.h"
#include "../../pointing/defs/pd_mode_flags.h"

#if __has_include("color.h")
#    include "color.h" // QMK
#endif

// ─── RGB config types ───────────────────────────────────────────────────────

typedef enum {
    RGB_BOTH_HALVES = 0,
    RGB_LEFT_HALF,
    RGB_RIGHT_HALF,
    RGB_KEY_HALF,
    RGB_KEYS_ONLY,
} rgb_locality_t;

typedef struct {
    pd_mode_mask_t pointing_mode;
    hsv_t          color;
    rgb_locality_t locality;
} pd_mode_color_t;

typedef enum {
    ALL_KEYS = 0,
    KEYS_MAPPED_ON_THIS_LAYER_ONLY,
} layer_color_mode_t;

typedef struct {
    hsv_t   color;
    uint8_t mode;
} layer_color_config_t;

typedef enum {
    FOLLOW_REAL_DESTINATION = 0,
    END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW,
    END_COLOR_ON_ALL_KEYS,
} automouse_fade_end_mode_t;

typedef struct {
    automouse_fade_end_mode_t mode;
    hsv_t                     end_color;
} automouse_fade_end_config_t;

typedef struct {
    hsv_t          color;
    rgb_locality_t locality;
} combo_feedback_color_config_t;

#ifndef NOAH_RGB_LED_GROUP_MAX_LEDS
#    ifdef RGB_MATRIX_LED_COUNT
#        define NOAH_RGB_LED_GROUP_MAX_LEDS RGB_MATRIX_LED_COUNT
#    else
#        define NOAH_RGB_LED_GROUP_MAX_LEDS 58
#    endif
#endif

typedef struct {
    uint8_t leds[NOAH_RGB_LED_GROUP_MAX_LEDS];
    uint8_t count;
} rgb_led_group_t;

#define RGB_LAYER_GROUP_ALL UINT8_MAX
#define RGB_PD_MODE_GROUP_ALL ((pd_mode_mask_t)0u)

static inline bool rgb_hsv_is_inherit_color(hsv_t color) {
    return color.h == 0 && color.s == 0 && color.v == 0;
}

typedef struct {
    hsv_t           color;
    rgb_led_group_t led_group;
} combo_feedback_led_group_t;

typedef struct {
    const hsv_t                   *tap_branch_colors;
    uint8_t                        tap_branch_color_count;
    hsv_t                          tap_committed_color;
    hsv_t                          hold_active_color;
    hsv_t                          long_hold_active_color;
    key_feedback_tap_commit_mode_t tap_commit_mode;
    rgb_locality_t                 locality;
} key_behavior_feedback_color_config_t;

typedef enum {
    KEY_FEEDBACK_GROUP_TAP_BRANCH_PENDING = 0,
    KEY_FEEDBACK_GROUP_TAP_COMMITTED,
    KEY_FEEDBACK_GROUP_HOLD_ACTIVE,
    KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE,
    KEY_FEEDBACK_GROUP_ALL,
} key_behavior_feedback_group_semantic_t;

typedef struct {
    key_behavior_feedback_group_semantic_t semantic;
    hsv_t                                  color;
    rgb_led_group_t                        led_group;
} key_behavior_feedback_led_group_t;

typedef struct {
    uint8_t         layer;
    hsv_t           color;
    rgb_led_group_t led_group;
} layer_led_group_t;

typedef struct {
    pd_mode_mask_t  pointing_mode;
    hsv_t           color;
    rgb_led_group_t led_group;
} pd_mode_led_group_t;

#if defined(RGB_MATRIX_ENABLE)

#    include "rgb_matrix.h" // QMK

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
// The split boundary comes from RGB_LEFT_LED_COUNT in users/noah/config.h, which
// also derives RGB_MATRIX_SPLIT, so the two cannot drift apart. The fallback
// below only applies to builds that do not include that config, such as focused
// host tests.

#    ifndef RGB_LEFT_LED_COUNT
#        define RGB_LEFT_LED_COUNT 29
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
