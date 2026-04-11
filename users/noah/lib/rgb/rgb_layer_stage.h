// ────────────────────────────────────────────────────────────────────────────
// RGB Layer Stage
// ────────────────────────────────────────────────────────────────────────────
//
// Shared layer-composition stage for the RGB runtime. This module owns the
// authored layer-key coverage map, frame helpers, and the base layer render
// pass so higher-level runtime code can focus on stage ordering.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "rgb_helpers.h"

#ifdef RGB_MATRIX_ENABLE
typedef struct {
    rgb_t colors[RGB_MATRIX_LED_COUNT];
    bool  painted[RGB_MATRIX_LED_COUNT];
} rgb_runtime_frame_t;

void rgb_runtime_layer_stage_post_init(void);
void rgb_runtime_layer_stage_invalidate_maps(void);
bool rgb_runtime_layer_stage_has_solid_color(uint8_t layer);
void rgb_runtime_frame_clear(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max);
bool rgb_runtime_frame_fill(rgb_runtime_frame_t *frame, rgb_t color, uint8_t led_min, uint8_t led_max);
bool rgb_runtime_layer_stage_render_frame(rgb_runtime_frame_t *frame, layer_state_t state, uint8_t led_min, uint8_t led_max);
bool rgb_runtime_layer_stage_apply_frame(const rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max);
bool rgb_runtime_layer_stage_paint_layer(uint8_t layer, uint8_t led_min, uint8_t led_max);
#endif
