// ────────────────────────────────────────────────────────────────────────────
// RGB Key Feedback Stage
// ────────────────────────────────────────────────────────────────────────────
//
// Shared key-feedback overlay stage. It composes packed runtime semantics with
// one captured effective profile frame before the top-level runtime applies
// any LEDs.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "rgb_layer_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE)
void rgb_runtime_key_feedback_stage_post_init(void);
bool rgb_runtime_key_feedback_stage_render_effective_frame(rgb_runtime_frame_t *frame, const noah_effective_rgb_frame_t *profile_frame, const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max);
#else
static inline void rgb_runtime_key_feedback_stage_post_init(void) {}
static inline bool rgb_runtime_key_feedback_stage_render_effective_frame(void *frame, const noah_effective_rgb_frame_t *profile_frame, const uint8_t *semantic_map, const uint8_t *tap_branch_map, const uint8_t *flash_visibility_bitmap, const uint8_t *broad_owner_map, uint8_t led_min, uint8_t led_max) {
    (void)frame;
    (void)profile_frame;
    (void)semantic_map;
    (void)tap_branch_map;
    (void)flash_visibility_bitmap;
    (void)broad_owner_map;
    (void)led_min;
    (void)led_max;
    return false;
}
#endif
