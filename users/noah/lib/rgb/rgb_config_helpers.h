// ────────────────────────────────────────────────────────────────────────────
// RGB Config Helpers
// ────────────────────────────────────────────────────────────────────────────
//
// Authoring helpers for keymap RGB config files. These macros keep counted
// config tables declarative without mixing the count plumbing into
// rgb_config.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "rgb_helpers.h"

#define HSV(h_, s_, v_) { .h = (h_), .s = (s_), .v = (v_) }

#define DEFINE_KEY_BEHAVIOR_FEEDBACK_COLORS(...)                                          \
    const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {__VA_ARGS__}

#define DEFINE_PD_MODE_COLORS(...)                                                  \
    const pd_mode_color_t pd_mode_colors[] = {__VA_ARGS__};                         \
    const uint8_t         pd_mode_color_count = (uint8_t)(sizeof(pd_mode_colors) / sizeof(pd_mode_colors[0]))

#define DEFINE_LAYER_LED_GROUPS(...)                                                            \
    static const layer_led_group_t layer_led_groups_data[] = {__VA_ARGS__};                     \
    const layer_led_group_t *const layer_led_groups        = layer_led_groups_data;             \
    const uint8_t                 layer_led_group_count    = (uint8_t)(sizeof(layer_led_groups_data) / sizeof(layer_led_groups_data[0]))

#define DEFINE_PD_MODE_LED_GROUPS(...)                                                            \
    static const pd_mode_led_group_t pd_mode_led_groups_data[] = {__VA_ARGS__};                  \
    const pd_mode_led_group_t *const pd_mode_led_groups        = pd_mode_led_groups_data;        \
    const uint8_t                    pd_mode_led_group_count   = (uint8_t)(sizeof(pd_mode_led_groups_data) / sizeof(pd_mode_led_groups_data[0]))
