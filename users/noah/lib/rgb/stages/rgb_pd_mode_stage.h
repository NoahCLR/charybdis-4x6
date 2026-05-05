// ────────────────────────────────────────────────────────────────────────────
// RGB PD-Mode Stage
// ────────────────────────────────────────────────────────────────────────────
//
// Shared pointing-device mode overlay stage for the RGB runtime. This module
// owns the pd-mode color cache plus active-mode/group rendering so the
// top-level runtime can keep shrinking toward pure stage ordering.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
void rgb_runtime_pd_mode_stage_post_init(void);
bool rgb_runtime_pd_mode_stage_render(uint8_t led_min, uint8_t led_max);
#else
static inline void rgb_runtime_pd_mode_stage_post_init(void) {}
static inline bool rgb_runtime_pd_mode_stage_render(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return false;
}
#endif
