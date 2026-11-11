// ────────────────────────────────────────────────────────────────────────────
// RGB Replacement Feedback Stage
// ────────────────────────────────────────────────────────────────────────────
//
// The flash that shows text replacement (autocorrect) replaced a typed form
// or undid a replacement (D-F70). It composes what the typing stage reports,
// or split sync mirrors of it, with one captured frame of the active profile
// before the top-level runtime applies any LEDs.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "rgb_layer_stage.h"
#include "../../autocorrect/autocorrect_feedback.h"

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_AUTOCORRECT_FEEDBACK_ENABLE)
bool rgb_runtime_autocorrect_feedback_stage_render_active_profile_frame(rgb_runtime_frame_t *frame, const noah_active_profile_rgb_frame_t *profile_frame, const noah_autocorrect_feedback_t *feedback, uint8_t led_min, uint8_t led_max);
#else
static inline bool rgb_runtime_autocorrect_feedback_stage_render_active_profile_frame(void *frame, const noah_active_profile_rgb_frame_t *profile_frame, const noah_autocorrect_feedback_t *feedback, uint8_t led_min, uint8_t led_max) {
    (void)frame;
    (void)profile_frame;
    (void)feedback;
    (void)led_min;
    (void)led_max;
    return false;
}
#endif
