// ────────────────────────────────────────────────────────────────────────────
// RGB Preview Stage
// ────────────────────────────────────────────────────────────────────────────
//
// Shared preview-layer overlay stage for the RGB runtime. This module owns
// preview-layer lookup plus preview-only LED-group painting so the top-level
// runtime can keep shrinking toward pure render-stage ordering.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)
bool rgb_runtime_preview_stage_render(uint8_t led_min, uint8_t led_max);
#else
static inline bool rgb_runtime_preview_stage_render(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return false;
}
#endif
