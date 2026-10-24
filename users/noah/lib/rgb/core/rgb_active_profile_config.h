// ────────────────────────────────────────────────────────────────────────────
// Active-Profile RGB Configuration Adapter
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rgb_helpers.h"
#include "../../profile/runtime/active_profile_rgb_runtime.h"

typedef struct {
    uint8_t selector;
    hsv_t   color;
    uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
} rgb_active_profile_layer_group_t;

typedef struct {
    uint8_t selector;
    hsv_t   color;
    uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
} rgb_active_profile_pd_group_t;

typedef struct {
    hsv_t   color;
    uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
} rgb_active_profile_combo_group_t;

typedef struct {
    hsv_t                          tap_branch_colors[NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS];
    uint8_t                        tap_branch_color_count;
    hsv_t                          tap_committed_color;
    hsv_t                          hold_active_color;
    hsv_t                          long_hold_active_color;
    key_feedback_tap_commit_mode_t tap_commit_mode;
    rgb_locality_t                 locality;
} rgb_active_profile_key_feedback_t;

typedef struct {
    uint8_t selector;
    hsv_t   color;
    uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
} rgb_active_profile_key_group_t;

noah_active_profile_rgb_result_t rgb_active_profile_config_capture_frame(noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_frame_current(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_layer_stage_enabled(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_layer_color(const noah_active_profile_rgb_frame_t *frame, uint8_t layer, layer_color_config_t *color);
uint8_t                          rgb_active_profile_config_layer_group_count(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_layer_group_at(const noah_active_profile_rgb_frame_t *frame, uint8_t index, rgb_active_profile_layer_group_t *group);
bool                             rgb_active_profile_config_automouse_stage_enabled(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_automouse(const noah_active_profile_rgb_frame_t *frame, automouse_fade_end_config_t *config);
bool                             rgb_active_profile_config_pd_stage_enabled(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_pd_color(const noah_active_profile_rgb_frame_t *frame, pointing_mode_id_t mode_id, pointing_mode_color_t *color);
uint8_t                          rgb_active_profile_config_pd_group_count(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_pd_group_at(const noah_active_profile_rgb_frame_t *frame, uint8_t index, rgb_active_profile_pd_group_t *group);
bool                             rgb_active_profile_config_combo_stage_enabled(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_combo_feedback(const noah_active_profile_rgb_frame_t *frame, combo_feedback_color_config_t *config);
uint8_t                          rgb_active_profile_config_combo_group_count(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_combo_group_at(const noah_active_profile_rgb_frame_t *frame, uint8_t index, rgb_active_profile_combo_group_t *group);
bool                             rgb_active_profile_config_key_stage_enabled(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_key_feedback(const noah_active_profile_rgb_frame_t *frame, rgb_active_profile_key_feedback_t *config);
uint8_t                          rgb_active_profile_config_key_group_count(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_key_group_at(const noah_active_profile_rgb_frame_t *frame, uint8_t index, rgb_active_profile_key_group_t *group);
