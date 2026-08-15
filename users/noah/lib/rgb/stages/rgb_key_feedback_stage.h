// ────────────────────────────────────────────────────────────────────────────
// RGB Key Feedback Stage
// ────────────────────────────────────────────────────────────────────────────
//
// Shared key-feedback overlay stage for the RGB runtime. This module owns the
// cached feedback colors plus packed-flag rendering so the top-level runtime
// can focus on stage ordering.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)
void rgb_runtime_key_feedback_stage_post_init(void);
bool rgb_runtime_key_feedback_stage_render(const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max);
#else
static inline void rgb_runtime_key_feedback_stage_post_init(void) {}
static inline bool rgb_runtime_key_feedback_stage_render(const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max) {
    (void)semantic_map;
    (void)tap_branch_map;
    (void)flash_visibility_bitmap;
    (void)broad_owner_map;
    (void)led_min;
    (void)led_max;
    return false;
}
#endif
