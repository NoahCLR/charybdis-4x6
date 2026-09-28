// ────────────────────────────────────────────────────────────────────────────
// RGB Combo Feedback Stage
// ────────────────────────────────────────────────────────────────────────────
//
// Shared combo-held feedback stage for the RGB runtime. The runtime renders
// this stage twice: once as an underlay for preview/PD-owning combos and once
// as a normal overlay for all other active combos.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "rgb_layer_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
bool rgb_runtime_combo_feedback_stage_render_effective_frame(rgb_runtime_frame_t *frame, const noah_effective_rgb_frame_t *profile_frame, const uint8_t *bitmap, uint8_t led_min, uint8_t led_max);
#else
static inline bool rgb_runtime_combo_feedback_stage_render_effective_frame(void *frame, const noah_effective_rgb_frame_t *profile_frame, const uint8_t *bitmap, uint8_t led_min, uint8_t led_max) {
    (void)frame;
    (void)profile_frame;
    (void)bitmap;
    (void)led_min;
    (void)led_max;
    return false;
}
#endif
