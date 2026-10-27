// ────────────────────────────────────────────────────────────────────────────
// RGB Pointing-Mode Stage
// ────────────────────────────────────────────────────────────────────────────
//
// Shared pointing-device mode overlay stage for the RGB runtime. This module
// owns active-mode and group composition from the active profile's frame so the
// top-level runtime can keep shrinking toward pure stage ordering.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "rgb_layer_stage.h"

#if defined(RGB_MATRIX_ENABLE) && defined(POINTING_DEVICE_ENABLE) && defined(RGB_POINTING_MODE_FEEDBACK_ENABLE)
// The layer the running mode paints directly beneath: the lowest layer of its
// live link that is on in `state` (D-F62), so every layer of the link that is
// on paints over it; POINTING_MODE_NO_LAYER when it paints above the layers as
// usual, or has no colour to paint (its stage off), so the layer stack stays
// whole. Both halves answer alike: the slave half reads the mirrored mode,
// its own slots and the synced layers.
uint8_t rgb_runtime_pointing_mode_stage_beneath_layer(layer_state_t state, const noah_active_profile_rgb_frame_t *profile_frame);
bool rgb_runtime_pointing_mode_stage_render_frame(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max);
bool rgb_runtime_pointing_mode_stage_render_active_profile_frame(rgb_runtime_frame_t *frame, const noah_active_profile_rgb_frame_t *profile_frame, uint8_t led_min, uint8_t led_max);
// The same, painted beneath `beneath_layer` from the layer stage: only while
// the running mode's link still names it. False when it painted nothing here,
// so stage 5 may paint the mode instead.
bool rgb_runtime_pointing_mode_stage_render_active_profile_beneath_frame(rgb_runtime_frame_t *frame, const noah_active_profile_rgb_frame_t *profile_frame, uint8_t beneath_layer, uint8_t led_min, uint8_t led_max);
#else
// No pointing lighting: no mode paints beneath a layer (UINT8_MAX is
// POINTING_MODE_NO_LAYER, from a header this build may not include).
static inline uint8_t rgb_runtime_pointing_mode_stage_beneath_layer(layer_state_t state, const noah_active_profile_rgb_frame_t *profile_frame) {
    (void)state;
    (void)profile_frame;
    return UINT8_MAX;
}
static inline bool rgb_runtime_pointing_mode_stage_render_frame(void *frame, uint8_t led_min, uint8_t led_max) {
    (void)frame;
    (void)led_min;
    (void)led_max;
    return false;
}
static inline bool rgb_runtime_pointing_mode_stage_render_active_profile_frame(void *frame, const noah_active_profile_rgb_frame_t *profile_frame, uint8_t led_min, uint8_t led_max) {
    (void)profile_frame;
    return rgb_runtime_pointing_mode_stage_render_frame(frame, led_min, led_max);
}
static inline bool rgb_runtime_pointing_mode_stage_render_active_profile_beneath_frame(void *frame, const noah_active_profile_rgb_frame_t *profile_frame, uint8_t beneath_layer, uint8_t led_min, uint8_t led_max) {
    (void)profile_frame;
    (void)beneath_layer;
    return rgb_runtime_pointing_mode_stage_render_frame(frame, led_min, led_max);
}
#endif
