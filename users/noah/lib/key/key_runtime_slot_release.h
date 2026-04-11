// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Release
// ────────────────────────────────────────────────────────────────────────────
//
// Release-specific slot transition contracts for the handled-key runtime.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_state.h"

typedef enum {
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP,
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_ACTION,
    KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP,
} key_runtime_slot_release_outcome_t;

typedef struct {
    bool                               release_owned_state;
    key_runtime_slot_release_outcome_t outcome;
    uint16_t                           action;
    pd_mode_mask_t                     pd_mode_lock_tap;
} key_runtime_slot_release_resolution_t;

typedef enum {
    KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_DISPATCH_ACTION,
    KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_PD_MODE_LOCK_TAP,
} key_runtime_slot_release_apply_outcome_t;

typedef struct {
    bool                                     handled;
    bool                                     release_layer;
    keypos_t                                 key_pos;
    bool                                     release_owned_state;
    key_runtime_slot_release_apply_outcome_t outcome;
    uint16_t                                 action;
    pd_mode_mask_t                           pd_mode_lock_tap;
} key_runtime_slot_release_apply_t;

typedef enum {
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_NONE = 0,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_DELAYED_ACTION,
    KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_HELD_LIFECYCLE,
} key_runtime_slot_pending_multi_tap_hold_release_outcome_t;

typedef struct {
    bool                                                    handled;
    bool                                                    release_layer_after_action;
    keypos_t                                                key_pos;
    key_runtime_slot_pending_multi_tap_hold_release_outcome_t outcome;
    uint16_t                                                action;
    delayed_action_mods_t                                   mods;
    uint8_t                                                 repeat_count;
} key_runtime_slot_pending_multi_tap_hold_release_t;

key_runtime_slot_release_resolution_t key_runtime_slot_resolve_release(uint16_t keycode, active_key_state_t released_key, key_behavior_view_t behavior, uint16_t elapsed);
key_runtime_slot_release_apply_t key_runtime_slot_take_active_release(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior);
key_runtime_slot_pending_multi_tap_hold_release_t key_runtime_slot_take_pending_multi_tap_hold_release(active_key_state_t *slot, uint16_t keycode, key_behavior_view_t behavior, uint16_t elapsed);
