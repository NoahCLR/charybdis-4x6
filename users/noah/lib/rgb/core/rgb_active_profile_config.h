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

// Behaviour feedback's tiers, in case order after the tap branches.
typedef enum {
    RGB_ACTIVE_PROFILE_KEY_TIER_TAP_COMMITTED = 0,
    RGB_ACTIVE_PROFILE_KEY_TIER_HOLD,
    RGB_ACTIVE_PROFILE_KEY_TIER_LONG_HOLD,
    RGB_ACTIVE_PROFILE_KEY_TIER_COUNT,
} rgb_active_profile_key_tier_t;

enum {
    RGB_ACTIVE_PROFILE_KEY_MAX_CASES = NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS + RGB_ACTIVE_PROFILE_KEY_TIER_COUNT,
};

// Behaviour feedback's cases in case order (D-F73): the tap branches pending,
// from tap count 2, then tap committed, hold and long hold. Case n is a tap
// branch below tap_branch_count; rgb_active_profile_key_tier_case() names a
// tier's.
typedef struct {
    hsv_t                          case_colors[RGB_ACTIVE_PROFILE_KEY_MAX_CASES];
    rgb_locality_t                 case_localities[RGB_ACTIVE_PROFILE_KEY_MAX_CASES];
    uint8_t                        tap_branch_count;
    key_feedback_tap_commit_mode_t tap_commit_mode;
} rgb_active_profile_key_feedback_t;

static inline uint8_t rgb_active_profile_key_tier_case(const rgb_active_profile_key_feedback_t *config, rgb_active_profile_key_tier_t tier) {
    return (uint8_t)(config->tap_branch_count + (uint8_t)tier);
}

typedef struct {
    uint8_t selector;
    hsv_t   color;
    uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
} rgb_active_profile_key_group_t;

typedef struct {
    // One colour and one locality per replacement-feedback semantic, in the
    // format's order (D-F73).
    hsv_t          colors[NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_COUNT];
    rgb_locality_t localities[NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_COUNT];
} rgb_active_profile_autocorrect_feedback_t;

typedef struct {
    uint8_t selector;
    hsv_t   color;
    uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
} rgb_active_profile_autocorrect_group_t;

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
bool                             rgb_active_profile_config_autocorrect_stage_enabled(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_autocorrect_feedback(const noah_active_profile_rgb_frame_t *frame, rgb_active_profile_autocorrect_feedback_t *config);
uint8_t                          rgb_active_profile_config_autocorrect_group_count(const noah_active_profile_rgb_frame_t *frame);
bool                             rgb_active_profile_config_autocorrect_group_at(const noah_active_profile_rgb_frame_t *frame, uint8_t index, rgb_active_profile_autocorrect_group_t *group);

// Each layer, pointing, behaviour-feedback and replacement-feedback group row's selector, read once per
// lighting publication (D-F63). A stage compares a row's selector here and
// reads the row and its group only when it matches, so an accent that is not
// lit costs no profile read. Rows keep their table order. Combo rows have no
// selector and are not here.
typedef struct {
    const uint8_t *layer;
    const uint8_t *pd;
    const uint8_t *key;
    const uint8_t *autocorrect;
    uint8_t        layer_count;
    uint8_t        pd_count;
    uint8_t        key_count;
    uint8_t        autocorrect_count;
} rgb_active_profile_group_selectors_t;

bool rgb_active_profile_config_group_selectors(const noah_active_profile_rgb_frame_t *frame, rgb_active_profile_group_selectors_t *selectors);
void rgb_active_profile_config_invalidate_group_selectors(void);

#ifdef RGB_ACTIVE_PROFILE_CONFIG_TEST_BACKEND
typedef struct {
    uint32_t selector_reads;
    uint32_t layer_rows;
    uint32_t pd_rows;
    uint32_t combo_rows;
    uint32_t key_rows;
    uint32_t autocorrect_rows;
} rgb_active_profile_config_test_reads_t;

void                                   rgb_active_profile_config_test_reset_reads(void);
rgb_active_profile_config_test_reads_t rgb_active_profile_config_test_reads(void);
#endif
