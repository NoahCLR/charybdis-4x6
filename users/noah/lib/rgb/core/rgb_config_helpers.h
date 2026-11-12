// ────────────────────────────────────────────────────────────────────────────
// RGB Config Helpers
// ────────────────────────────────────────────────────────────────────────────
//
// Minimal authoring helpers for keymap RGB config files.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "rgb_helpers.h"
#include "lib/profile/schema/profile_rgb_compiled_v1.h"

#define HSV(h_, s_, v_) {.h = (h_), .s = (s_), .v = (v_)}
// The tap branches pending, from tap count 2, each {.color, .locality}.
#define RGB_TAP_BRANCHES(...) .tap_branches = (const key_behavior_feedback_tap_branch_t[]){__VA_ARGS__}, .tap_branch_count = (uint8_t)ARRAY_SIZE(((const key_behavior_feedback_tap_branch_t[]){__VA_ARGS__}))
#define RGB_LED_GROUP(...) {.leds = {__VA_ARGS__}, .count = (uint8_t)ARRAY_SIZE(((const uint8_t[]){__VA_ARGS__}))}
#define _RGB_LED_GROUP_TABLE_END {.led_group = {.count = 0}}
#define RGB_LED_GROUP_TABLE(...) {__VA_ARGS__ _RGB_LED_GROUP_TABLE_END}
#define RGB_SAVED_LED_GROUP_TABLE(...) {__VA_ARGS__{.count = 0}}

// A lighting table holds at most 255 group rows, its header byte (D-F63).
#define _RGB_ASSERT_TABLE_ROWS(rows_) _Static_assert((rows_) <= NOAH_PROFILE_RGB_V1_MAX_TABLE_GROUP_ROWS, "a lighting table holds at most 255 group rows (D-F63)")

#define EXPORT_LAYER_LED_GROUPS(groups_)                              \
    _RGB_ASSERT_TABLE_ROWS(sizeof(groups_) / sizeof((groups_)[0]));   \
    const layer_led_group_t *const layer_led_groups      = (groups_); \
    const uint8_t                  layer_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_POINTING_MODE_LED_GROUPS(groups_)                                      \
    _RGB_ASSERT_TABLE_ROWS(sizeof(groups_) / sizeof((groups_)[0]));                   \
    const pointing_mode_led_group_t *const pointing_mode_led_groups      = (groups_); \
    const uint8_t                          pointing_mode_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_COMBO_FEEDBACK_LED_GROUPS(groups_)                                       \
    _RGB_ASSERT_TABLE_ROWS(sizeof(groups_) / sizeof((groups_)[0]));                     \
    const combo_feedback_led_group_t *const combo_feedback_led_groups      = (groups_); \
    const uint8_t                           combo_feedback_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_KEY_BEHAVIOR_FEEDBACK_LED_GROUPS(groups_)                                              \
    _RGB_ASSERT_TABLE_ROWS(sizeof(groups_) / sizeof((groups_)[0]));                                   \
    const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups      = (groups_); \
    const uint8_t                                  key_behavior_feedback_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))

#define EXPORT_AUTOCORRECT_FEEDBACK_LED_GROUPS(groups_)                                             \
    _RGB_ASSERT_TABLE_ROWS(sizeof(groups_) / sizeof((groups_)[0]));                                 \
    const autocorrect_feedback_led_group_t *const autocorrect_feedback_led_groups      = (groups_); \
    const uint8_t                                 autocorrect_feedback_led_group_count = (uint8_t)(sizeof(groups_) / sizeof((groups_)[0]))
#define EXPORT_SAVED_LED_GROUP_TABLE(groups_)                                                                                                                   \
    _Static_assert(ARRAY_SIZE(groups_) - 1u <= NOAH_PROFILE_RGB_COMPILED_V1_MAX_SAVED_GROUPS, "the compiled lighting holds at most 16 saved LED sets (D-F63)"); \
    const rgb_led_group_t *const saved_led_groups      = (groups_);                                                                                             \
    const uint8_t                saved_led_group_count = (uint8_t)(ARRAY_SIZE(groups_) - 1u)

#define EXPORT_LAYER_LED_GROUP_TABLE(groups_)                         \
    _RGB_ASSERT_TABLE_ROWS(ARRAY_SIZE(groups_) - 1u);                 \
    const layer_led_group_t *const layer_led_groups      = (groups_); \
    const uint8_t                  layer_led_group_count = (uint8_t)(ARRAY_SIZE(groups_) - 1u)

#define EXPORT_POINTING_MODE_LED_GROUP_TABLE(groups_)                                 \
    _RGB_ASSERT_TABLE_ROWS(ARRAY_SIZE(groups_) - 1u);                                 \
    const pointing_mode_led_group_t *const pointing_mode_led_groups      = (groups_); \
    const uint8_t                          pointing_mode_led_group_count = (uint8_t)(ARRAY_SIZE(groups_) - 1u)

#define EXPORT_COMBO_FEEDBACK_LED_GROUP_TABLE(groups_)                                  \
    _RGB_ASSERT_TABLE_ROWS(ARRAY_SIZE(groups_) - 1u);                                   \
    const combo_feedback_led_group_t *const combo_feedback_led_groups      = (groups_); \
    const uint8_t                           combo_feedback_led_group_count = (uint8_t)(ARRAY_SIZE(groups_) - 1u)

#define EXPORT_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_TABLE(groups_)                                         \
    _RGB_ASSERT_TABLE_ROWS(ARRAY_SIZE(groups_) - 1u);                                                 \
    const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups      = (groups_); \
    const uint8_t                                  key_behavior_feedback_led_group_count = (uint8_t)(ARRAY_SIZE(groups_) - 1u)

#define EXPORT_AUTOCORRECT_FEEDBACK_LED_GROUP_TABLE(groups_)                                        \
    _RGB_ASSERT_TABLE_ROWS(ARRAY_SIZE(groups_) - 1u);                                               \
    const autocorrect_feedback_led_group_t *const autocorrect_feedback_led_groups      = (groups_); \
    const uint8_t                                 autocorrect_feedback_led_group_count = (uint8_t)(ARRAY_SIZE(groups_) - 1u)

// The compiled lighting's encoding, kept once by profile_rgb_compiled_v1.c in
// a buffer sized here to the authored tables rather than to the domain's
// maximum: fixed records, at most one LED set per saved group or row, the
// layer colours and rows, then each compiled stage's
// colours and rows. The counts are the tables' row counts.
#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_POINTING_MODE_FEEDBACK_ENABLE)
#    define _RGB_COMPILED_POINTING_BYTES(rows_) (NOAH_PROFILE_RGB_V1_MAX_POINTING_MODES * NOAH_PROFILE_RGB_V1_PD_COLOR_RECORD_SIZE + (rows_) * NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE)
#else
#    define _RGB_COMPILED_POINTING_BYTES(rows_) 0u
#endif
#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
#    define _RGB_COMPILED_COMBO_BYTES(rows_) ((rows_) * NOAH_PROFILE_RGB_V1_COMBO_GROUP_RECORD_SIZE)
#else
#    define _RGB_COMPILED_COMBO_BYTES(rows_) 0u
#endif
#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#    define _RGB_COMPILED_KEY_BYTES(rows_) (NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS * NOAH_PROFILE_RGB_V1_TAP_BRANCH_RECORD_SIZE + (rows_) * NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE)
#else
#    define _RGB_COMPILED_KEY_BYTES(rows_) 0u
#endif
#ifdef RGB_AUTOCORRECT_FEEDBACK_ENABLE
#    define _RGB_COMPILED_AUTOCORRECT_BYTES(rows_) ((rows_) * NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE)
#else
#    define _RGB_COMPILED_AUTOCORRECT_BYTES(rows_) 0u
#endif
#define RGB_COMPILED_CACHE_BYTES(saved_, layer_, pd_, combo_, key_, autocorrect_) (NOAH_PROFILE_RGB_V1_FIXED_PAYLOAD_SIZE + ((saved_) + (layer_) + (pd_) + (combo_) + (key_) + (autocorrect_)) * NOAH_PROFILE_RGB_V1_GROUP_RECORD_SIZE + LAYER_COUNT * NOAH_PROFILE_RGB_V1_LAYER_COLOR_RECORD_SIZE + (layer_) * NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE + _RGB_COMPILED_POINTING_BYTES(pd_) + _RGB_COMPILED_COMBO_BYTES(combo_) + _RGB_COMPILED_KEY_BYTES(key_) + _RGB_COMPILED_AUTOCORRECT_BYTES(autocorrect_))

#define EXPORT_RGB_COMPILED_CACHE(saved_, layer_, pd_, combo_, key_, autocorrect_)                                                                                                                                                                                     \
    _Static_assert((saved_) <= NOAH_PROFILE_RGB_COMPILED_V1_MAX_SAVED_GROUPS && (layer_) + (pd_) + (combo_) + (key_) + (autocorrect_) <= NOAH_PROFILE_RGB_COMPILED_V1_MAX_GROUP_ROWS, "the compiled lighting holds at most 16 saved LED sets and 32 group rows (D-F63)"); \
    _Static_assert(RGB_COMPILED_CACHE_BYTES(saved_, layer_, pd_, combo_, key_, autocorrect_) <= NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE, "the compiled lighting fits its domain");                                                                                        \
    static uint8_t noah_rgb_compiled_cache_storage[RGB_COMPILED_CACHE_BYTES(saved_, layer_, pd_, combo_, key_, autocorrect_)];                                                                                                                                         \
    uint8_t *const noah_rgb_compiled_cache      = noah_rgb_compiled_cache_storage;                                                                                                                                                                       \
    const uint16_t noah_rgb_compiled_cache_size = (uint16_t)sizeof(noah_rgb_compiled_cache_storage)

#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_POINTING_MODE_FEEDBACK_ENABLE)
#    define _RGB_POINTING_MODE_COLOR_COUNT_DATA() const uint8_t pointing_mode_color_count = (uint8_t)ARRAY_SIZE(pointing_mode_colors);
#    define _RGB_POINTING_MODE_LED_GROUP_DATA() EXPORT_POINTING_MODE_LED_GROUP_TABLE(pointing_mode_led_groups_data);
#    define _RGB_POINTING_MODE_LED_GROUP_ROWS (ARRAY_SIZE(pointing_mode_led_groups_data) - 1u)
#else
#    define _RGB_POINTING_MODE_COLOR_COUNT_DATA()
#    define _RGB_POINTING_MODE_LED_GROUP_DATA()
#    define _RGB_POINTING_MODE_LED_GROUP_ROWS 0u
#endif

#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
#    define _RGB_COMBO_FEEDBACK_LED_GROUP_DATA() EXPORT_COMBO_FEEDBACK_LED_GROUP_TABLE(combo_feedback_led_groups_data);
#    define _RGB_COMBO_FEEDBACK_LED_GROUP_ROWS (ARRAY_SIZE(combo_feedback_led_groups_data) - 1u)
#else
#    define _RGB_COMBO_FEEDBACK_LED_GROUP_DATA()
#    define _RGB_COMBO_FEEDBACK_LED_GROUP_ROWS 0u
#endif

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#    define _RGB_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_DATA() EXPORT_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_TABLE(key_behavior_feedback_led_groups_data);
#    define _RGB_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_ROWS (ARRAY_SIZE(key_behavior_feedback_led_groups_data) - 1u)
#else
#    define _RGB_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_DATA()
#    define _RGB_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_ROWS 0u
#endif

#ifdef RGB_AUTOCORRECT_FEEDBACK_ENABLE
#    define _RGB_AUTOCORRECT_FEEDBACK_LED_GROUP_DATA() EXPORT_AUTOCORRECT_FEEDBACK_LED_GROUP_TABLE(autocorrect_feedback_led_groups_data);
#    define _RGB_AUTOCORRECT_FEEDBACK_LED_GROUP_ROWS (ARRAY_SIZE(autocorrect_feedback_led_groups_data) - 1u)
#else
#    define _RGB_AUTOCORRECT_FEEDBACK_LED_GROUP_DATA()
#    define _RGB_AUTOCORRECT_FEEDBACK_LED_GROUP_ROWS 0u
#endif
#define MATERIALIZE_RGB_CONFIG()                         \
    _RGB_POINTING_MODE_COLOR_COUNT_DATA()                \
    _RGB_POINTING_MODE_LED_GROUP_DATA()                  \
    _RGB_COMBO_FEEDBACK_LED_GROUP_DATA()                 \
    _RGB_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_DATA()          \
    _RGB_AUTOCORRECT_FEEDBACK_LED_GROUP_DATA()           \
    EXPORT_SAVED_LED_GROUP_TABLE(saved_led_groups_data); \
    EXPORT_LAYER_LED_GROUP_TABLE(layer_led_groups_data); \
    EXPORT_RGB_COMPILED_CACHE(ARRAY_SIZE(saved_led_groups_data) - 1u, ARRAY_SIZE(layer_led_groups_data) - 1u, _RGB_POINTING_MODE_LED_GROUP_ROWS, _RGB_COMBO_FEEDBACK_LED_GROUP_ROWS, _RGB_KEY_BEHAVIOR_FEEDBACK_LED_GROUP_ROWS, _RGB_AUTOCORRECT_FEEDBACK_LED_GROUP_ROWS)
