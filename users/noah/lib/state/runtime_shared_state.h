// ────────────────────────────────────────────────────────────────────────────
// Runtime Shared State
// ────────────────────────────────────────────────────────────────────────────
//
// Central runtime-owned state shared across the split key engine and pd-mode
// modules. This keeps storage ownership in one place so the higher-level
// runtime can evolve without more file-local globals leaking across modules.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../key/key_behavior.h"
#include "../key/multi_tap_engine.h"
#include "../pointing/pd_mode_flags.h"

typedef struct {
    uint16_t        timer;
    uint16_t        keycode;
    keypos_t        key_pos;
    bool            hold_fired;
    uint16_t        held_action_keycode;
    uint16_t        tap_action;
    uint16_t        tap_hold_term;
    uint16_t        longer_hold_term;
    uint16_t        multi_tap_term;
    bool            hold_one_shot_fired;
    bool            implicit_hold;
    bool            passthrough_modifier_pending;
    bool            pd_mode_was_locked_on_press;
    bool            layer_interrupted;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
} active_key_state_t;

typedef struct {
    uint16_t timer;
    bool     active;
    bool     long_hold_level;
} key_runtime_feedback_state_t;

#define ACTIVE_KEY_STATE_INIT                           \
    {                                                   \
        .keycode             = KC_NO,                   \
        .held_action_keycode = KC_NO,                   \
        .tap_hold_term       = CUSTOM_TAP_HOLD_TERM,    \
        .longer_hold_term    = CUSTOM_LONGER_HOLD_TERM, \
        .multi_tap_term      = CUSTOM_MULTI_TAP_TERM,   \
    }

typedef struct {
    active_key_state_t          active_key;
    multi_tap_t                 multi_tap;
    key_runtime_feedback_state_t feedback;
} key_runtime_shared_state_t;

typedef struct {
    pd_mode_mask_t active_flags;
    pd_mode_mask_t locked_flags;
} pd_mode_runtime_shared_state_t;

typedef struct {
    key_runtime_shared_state_t     key;
    pd_mode_runtime_shared_state_t pd;
} runtime_shared_state_t;

extern runtime_shared_state_t noah_runtime_shared_state;
