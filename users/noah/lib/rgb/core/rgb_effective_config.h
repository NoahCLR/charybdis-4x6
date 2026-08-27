// ────────────────────────────────────────────────────────────────────────────
// Effective RGB Configuration Adapter
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rgb_helpers.h"
#include "../../profile/runtime/effective_rgb_runtime.h"

typedef struct {
    uint8_t selector;
    hsv_t   color;
    uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
} rgb_effective_layer_group_t;

typedef struct {
    uint8_t selector;
    hsv_t   color;
    uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
} rgb_effective_pd_group_t;

noah_effective_rgb_result_t rgb_effective_config_capture_frame(noah_effective_rgb_frame_t *frame);
bool rgb_effective_config_frame_current(const noah_effective_rgb_frame_t *frame);
bool rgb_effective_config_layer_stage_enabled(const noah_effective_rgb_frame_t *frame);
bool rgb_effective_config_layer_color(const noah_effective_rgb_frame_t *frame, uint8_t layer, layer_color_config_t *color);
uint8_t rgb_effective_config_layer_group_count(const noah_effective_rgb_frame_t *frame);
bool rgb_effective_config_layer_group_at(const noah_effective_rgb_frame_t *frame, uint8_t index, rgb_effective_layer_group_t *group);
bool rgb_effective_config_pd_stage_enabled(const noah_effective_rgb_frame_t *frame);
bool rgb_effective_config_pd_color(const noah_effective_rgb_frame_t *frame, pd_mode_id_t mode_id, pd_mode_color_t *color);
uint8_t rgb_effective_config_pd_group_count(const noah_effective_rgb_frame_t *frame);
bool rgb_effective_config_pd_group_at(const noah_effective_rgb_frame_t *frame, uint8_t index, rgb_effective_pd_group_t *group);
