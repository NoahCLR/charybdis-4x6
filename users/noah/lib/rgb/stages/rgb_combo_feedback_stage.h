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

#if defined(RGB_MATRIX_ENABLE) && defined(COMBO_ENABLE)
void rgb_runtime_combo_feedback_stage_post_init(void);
bool rgb_runtime_combo_feedback_stage_render_underlay(uint8_t led_min, uint8_t led_max);
bool rgb_runtime_combo_feedback_stage_render_overlay(uint8_t led_min, uint8_t led_max);
#else
static inline void rgb_runtime_combo_feedback_stage_post_init(void) {}
static inline bool rgb_runtime_combo_feedback_stage_render_underlay(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return false;
}
static inline bool rgb_runtime_combo_feedback_stage_render_overlay(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return false;
}
#endif
