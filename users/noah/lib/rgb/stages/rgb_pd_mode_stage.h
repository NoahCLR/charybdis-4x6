// ────────────────────────────────────────────────────────────────────────────
// RGB PD-Mode Stage
// ────────────────────────────────────────────────────────────────────────────
//
// Shared pointing-device mode overlay stage for the RGB runtime. This module
// owns effective active-mode/group frame composition so the top-level runtime
// can keep shrinking toward pure stage ordering.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "rgb_layer_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
void rgb_runtime_pd_mode_stage_post_init(void);
bool rgb_runtime_pd_mode_stage_render_frame(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max);
bool rgb_runtime_pd_mode_stage_render_effective_frame(rgb_runtime_frame_t *frame, const noah_effective_rgb_frame_t *profile_frame, uint8_t led_min, uint8_t led_max);
#else
static inline void rgb_runtime_pd_mode_stage_post_init(void) {}
static inline bool rgb_runtime_pd_mode_stage_render_frame(void *frame, uint8_t led_min, uint8_t led_max) {
    (void)frame;
    (void)led_min;
    (void)led_max;
    return false;
}
static inline bool rgb_runtime_pd_mode_stage_render_effective_frame(void *frame, const noah_effective_rgb_frame_t *profile_frame, uint8_t led_min, uint8_t led_max) {
    (void)profile_frame;
    return rgb_runtime_pd_mode_stage_render_frame(frame, led_min, led_max);
}
#endif
