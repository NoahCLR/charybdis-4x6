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

typedef enum {
    KEY_RUNTIME_SLOT_PHASE_IDLE = 0,
    KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
    KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
    KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING,
    KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE,
    KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
} key_runtime_slot_phase_t;

typedef enum {
    KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT = 0,
    KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT,
    KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK,
} key_runtime_slot_hold_strategy_t;

typedef struct {
    uint16_t keycode;
    keypos_t key_pos;
} key_runtime_slot_owner_state_t;

typedef struct {
    key_runtime_slot_phase_t         phase;
    uint16_t                         held_action_keycode;
    bool                             repeat_binding_active;
    key_runtime_slot_hold_strategy_t hold_strategy;
    bool                             pd_mode_was_locked_on_press;
    bool                             layer_interrupted;
} key_runtime_slot_lifecycle_state_t;

typedef struct {
    uint16_t        tap_action;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
} key_runtime_slot_binding_state_t;

typedef struct {
    uint16_t tap_hold_term;
    uint16_t longer_hold_term;
    uint16_t multi_tap_term;
} key_runtime_slot_timing_state_t;

typedef struct {
    bool          valid;
    bool          has_multi_tap;
    bool          is_momentary_layer;
    bool          is_layer_tap;
    uint8_t       layer;
    uint8_t       preview_layer;
    pd_mode_mask_t pd_mode;
} key_runtime_slot_semantic_state_t;

// One handled-key runtime slot: active press/hold state plus any deferred
// multi-tap chain that still owns this physical key position after release.
typedef struct {
    uint16_t timer;
    key_runtime_slot_owner_state_t     owner;
    key_runtime_slot_lifecycle_state_t lifecycle;
    key_runtime_slot_binding_state_t   binding;
    key_runtime_slot_timing_state_t    timing;
    key_runtime_slot_semantic_state_t  semantic;
    multi_tap_t pending_multi_tap;
} active_key_state_t;

#define KEY_RUNTIME_SLOT_TABLE_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))

typedef active_key_state_t key_runtime_slot_state_t;

typedef struct {
    uint16_t timer;
    bool     active;
    bool     long_hold_level;
} key_runtime_feedback_state_t;

#define ACTIVE_KEY_STATE_INIT                        \
    {                                                \
        .owner.keycode                 = KC_NO,      \
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_IDLE, \
        .lifecycle.held_action_keycode = KC_NO,      \
        .timing.tap_hold_term          = CUSTOM_TAP_HOLD_TERM, \
        .timing.longer_hold_term       = CUSTOM_LONGER_HOLD_TERM, \
        .timing.multi_tap_term         = CUSTOM_MULTI_TAP_TERM, \
        .semantic.layer                = UINT8_MAX,  \
        .semantic.preview_layer        = UINT8_MAX,  \
        .pending_multi_tap             = {0},        \
    }

typedef struct {
    key_runtime_slot_state_t     slots_by_position[KEY_RUNTIME_SLOT_TABLE_CAPACITY];
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

void runtime_shared_state_reset(runtime_shared_state_t *state);
