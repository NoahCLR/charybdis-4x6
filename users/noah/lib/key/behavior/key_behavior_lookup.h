// ────────────────────────────────────────────────────────────────────────────
// Key Behavior Lookup
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime helpers that interpret authored key_behavior rows into resolved
// behavior views for the key engine.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../pointing/defs/pd_mode_flags.h"
#include "key_behavior.h"

typedef struct {
    const key_behavior_t *config;
    uint16_t              keycode;
    bool                  handled;
    bool                  is_momentary_layer; // MO() or authored LT() row
    bool                  is_layer_tap;       // specifically authored LT() row
    bool                  has_multi_tap;
    uint16_t              tap_hold_term;       // resolved: per-key → TAPPING_TERM for authored LT() → CUSTOM_TAP_HOLD_TERM
    uint16_t              longer_hold_term;    // resolved: per-key → CUSTOM_LONGER_HOLD_TERM
    uint16_t              multi_tap_term;      // resolved: per-key → CUSTOM_MULTI_TAP_TERM
    uint16_t              branch_confirm_term; // resolved RGB branch-confirm term; 0 = skipped
    key_behavior_step_t   single;
} key_behavior_view_t;

#ifdef KEY_BEHAVIOR_LOOKUP_TEST_INSTRUMENTATION
typedef struct {
    uint16_t search_count;
    uint16_t row_comparison_count;
} key_behavior_lookup_test_counters_t;

void key_behavior_lookup_test_counters_reset(void);
void key_behavior_lookup_test_counters_snapshot(key_behavior_lookup_test_counters_t *out);
#endif

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count);
bool                key_behavior_has_more_taps(uint16_t keycode, uint8_t count);
bool                key_behavior_keeps_auto_mouse_anchored(uint16_t keycode);
bool                key_behavior_future_tap_path_has_foreign_pd_mode(uint16_t keycode, uint8_t count, pd_mode_mask_t base_mode);
key_behavior_view_t key_behavior_lookup(uint16_t keycode);
key_behavior_step_t key_behavior_view_step(const key_behavior_view_t *behavior, uint8_t tap_count);
bool                key_behavior_view_has_more_taps(const key_behavior_view_t *behavior, uint8_t count);
uint8_t             key_behavior_validate_all(void);
