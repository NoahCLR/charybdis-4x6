// ────────────────────────────────────────────────────────────────────────────
// Runtime V2 Release Planning Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal reducer-owned release-resolution helpers used while the production
// active-release path migrates away from slot-owned release semantics.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "runtime_v2.h"

#include "../key/runtime/slot/key_runtime_slot_release_resolver.h"

typedef struct {
    key_runtime_slot_interaction_t      interaction;
    key_runtime_slot_phase_t            phase;
    uint16_t                            elapsed;
    bool                                held_action_active;
    bool                                repeat_active;
    bool                                momentary_layer_tap_interrupted;
    bool                                quick_tap;
    bool                                quick_immediate_hold;
    bool                                buffered_base_tap;
    pd_mode_mask_t                      lock_tap_mode;
    key_runtime_slot_release_decision_t decision;
} runtime_v2_active_release_resolution_t;

bool runtime_v2_resolve_active_release(keypos_t key_pos, runtime_v2_active_release_resolution_t *out);
