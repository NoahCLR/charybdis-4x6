// ────────────────────────────────────────────────────────────────────────────
// RGB Config Helpers
// ────────────────────────────────────────────────────────────────────────────
//
// Minimal authoring helpers for keymap RGB config files.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "rgb_helpers.h"

#define HSV(h_, s_, v_) {.h = (h_), .s = (s_), .v = (v_)}

#define EXPORT_LAYER_LED_GROUPS(groups_)                              \
    const layer_led_group_t *const layer_led_groups      = (groups_); \
    const uint8_t                  layer_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_PD_MODE_LED_GROUPS(groups_)                                \
    const pd_mode_led_group_t *const pd_mode_led_groups      = (groups_); \
    const uint8_t                    pd_mode_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_COMBO_FEEDBACK_LED_GROUPS(groups_)                                            \
    const combo_feedback_led_group_t *const combo_feedback_led_groups      = (groups_);       \
    const uint8_t                           combo_feedback_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_KEY_BEHAVIOR_FEEDBACK_LED_GROUPS(groups_)                                                       \
    const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups      = (groups_);          \
    const uint8_t                                  key_behavior_feedback_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))
