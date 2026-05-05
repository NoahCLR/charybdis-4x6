// ────────────────────────────────────────────────────────────────────────────
// RGB Automouse Stage
// ────────────────────────────────────────────────────────────────────────────
//
// Shared auto-mouse fade stage for the RGB runtime. This module owns the
// auto-mouse destination cache, base-effect capture, and frame blending so the
// top-level runtime can focus on render-stage ordering.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
void rgb_runtime_automouse_stage_post_init(void);
bool rgb_runtime_automouse_stage_should_render(layer_state_t state);
bool rgb_runtime_automouse_stage_render(layer_state_t state, uint8_t led_min, uint8_t led_max);
#else
static inline void rgb_runtime_automouse_stage_post_init(void) {}
static inline bool rgb_runtime_automouse_stage_should_render(layer_state_t state) {
    (void)state;
    return false;
}
static inline bool rgb_runtime_automouse_stage_render(layer_state_t state, uint8_t led_min, uint8_t led_max) {
    (void)state;
    (void)led_min;
    (void)led_max;
    return false;
}
#endif
