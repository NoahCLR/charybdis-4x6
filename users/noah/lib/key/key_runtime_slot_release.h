// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Release
// ────────────────────────────────────────────────────────────────────────────
//
// Release-specific slot transition helpers that remain externally useful
// outside the internal slot-result adapter.
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

key_runtime_slot_release_resolution_t key_runtime_slot_resolve_release(uint16_t keycode, active_key_state_t released_key, key_behavior_view_t behavior, uint16_t elapsed);
