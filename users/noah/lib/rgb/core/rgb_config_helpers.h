// ────────────────────────────────────────────────────────────────────────────
// RGB Config Helpers
// ────────────────────────────────────────────────────────────────────────────
//
// Minimal authoring helpers for keymap RGB config files.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "rgb_helpers.h"

#define HSV(h_, s_, v_) {.h = (h_), .s = (s_), .v = (v_)}
#define RGB_TAP_BRANCH_COLORS(...) .tap_branch_colors = (const hsv_t[]){__VA_ARGS__}, .tap_branch_color_count = (uint8_t)ARRAY_SIZE(((const hsv_t[]){__VA_ARGS__}))
#define RGB_LED_GROUP(...) ((rgb_led_group_t){.leds = (const uint8_t[]){__VA_ARGS__}, .count = (uint8_t)ARRAY_SIZE(((const uint8_t[]){__VA_ARGS__}))})
#define _RGB_LED_GROUP_TABLE_END {.led_group = {.count = 0}}
#define RGB_LED_GROUP_TABLE(...) {__VA_ARGS__ _RGB_LED_GROUP_TABLE_END}

#define EXPORT_LAYER_LED_GROUPS(groups_)                              \
    const layer_led_group_t *const layer_led_groups      = (groups_); \
    const uint8_t                  layer_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_PD_MODE_LED_GROUPS(groups_)                                \
    const pd_mode_led_group_t *const pd_mode_led_groups      = (groups_); \
    const uint8_t                    pd_mode_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_COMBO_FEEDBACK_LED_GROUPS(groups_)                                       \
    const combo_feedback_led_group_t *const combo_feedback_led_groups      = (groups_); \
    const uint8_t                           combo_feedback_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_KEY_BEHAVIOR_FEEDBACK_LED_GROUPS(groups_)                                              \
    const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups      = (groups_); \
    const uint8_t                                  key_behavior_feedback_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_LAYER_LED_GROUP_TABLE(groups_)                         \
    const layer_led_group_t *const layer_led_groups      = (groups_); \
    const uint8_t                  layer_led_group_count = (uint8_t)(ARRAY_SIZE(groups_) - 1u)

#define EXPORT_PD_MODE_LED_GROUP_TABLE(groups_)                           \
    const pd_mode_led_group_t *const pd_mode_led_groups      = (groups_); \
    const uint8_t                    pd_mode_led_group_count = (uint8_t)(ARRAY_SIZE(groups_) - 1u)

#define EXPORT_COMBO_FEEDBACK_LED_GROUP_TABLE(groups_)                                  \
    const combo_feedback_led_group_t *const combo_feedback_led_groups      = (groups_); \
    const uint8_t                           combo_feedback_led_group_count = (uint8_t)(ARRAY_SIZE(groups_) - 1u)

#define EXPORT_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_TABLE(groups_)                                         \
    const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups      = (groups_); \
    const uint8_t                                  key_behavior_feedback_led_group_count = (uint8_t)(ARRAY_SIZE(groups_) - 1u)

#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
#    define _RGB_PD_MODE_COLOR_COUNT_DATA() const uint8_t pd_mode_color_count = (uint8_t)ARRAY_SIZE(pd_mode_colors);
#    define _RGB_PD_MODE_LED_GROUP_DATA() EXPORT_PD_MODE_LED_GROUP_TABLE(pd_mode_led_groups_data);
#else
#    define _RGB_PD_MODE_COLOR_COUNT_DATA()
#    define _RGB_PD_MODE_LED_GROUP_DATA()
#endif

#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
#    define _RGB_COMBO_FEEDBACK_LED_GROUP_DATA() EXPORT_COMBO_FEEDBACK_LED_GROUP_TABLE(combo_feedback_led_groups_data);
#else
#    define _RGB_COMBO_FEEDBACK_LED_GROUP_DATA()
#endif

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#    define _RGB_KEY_BEHAVIOR_FEEDBACK_POLICY_DATA()                                      \
        key_feedback_branch_confirm_mode_t key_feedback_branch_confirm_mode(void) {       \
            return key_behavior_feedback_colors.branch_confirm_mode;                      \
        }                                                                                 \
        key_feedback_tap_commit_mode_t key_feedback_tap_commit_mode(void) {               \
            return key_behavior_feedback_colors.tap_commit_mode;                          \
        }
#    define _RGB_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_DATA() EXPORT_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_TABLE(key_behavior_feedback_led_groups_data);
#else
#    define _RGB_KEY_BEHAVIOR_FEEDBACK_POLICY_DATA()
#    define _RGB_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_DATA()
#endif

#define MATERIALIZE_RGB_CONFIG()                \
    _RGB_PD_MODE_COLOR_COUNT_DATA()             \
    _RGB_PD_MODE_LED_GROUP_DATA()               \
    _RGB_COMBO_FEEDBACK_LED_GROUP_DATA()        \
    _RGB_KEY_BEHAVIOR_FEEDBACK_POLICY_DATA()    \
    _RGB_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_DATA() \
    EXPORT_LAYER_LED_GROUP_TABLE(layer_led_groups_data)
