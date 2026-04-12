// ────────────────────────────────────────────────────────────────────────────
// Runtime Debug Snapshot
// ────────────────────────────────────────────────────────────────────────────
//
// Aggregate read-only snapshot and hard reset helpers for the userspace-owned
// runtime state. Higher-level host tests can use this surface instead of
// rebuilding partial reset logic across key runtime, pd modes, layer
// ownership, held-action ownership, held-repeat scheduling, and modifier
// ownership modules.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../key/held_action.h"
#include "../key/held_repeat.h"
#include "keyboard_mod_ownership.h"
#include "layer_ownership.h"
#include "runtime_shared_state.h"

typedef struct {
    runtime_shared_state_t                  core;
    layer_ownership_debug_snapshot_t        layer_ownership;
    held_action_debug_snapshot_t            held_actions;
    held_repeat_debug_snapshot_t            held_repeats;
    keyboard_mod_ownership_debug_snapshot_t keyboard_mod_ownership;
} noah_runtime_debug_snapshot_t;

void noah_runtime_debug_snapshot(noah_runtime_debug_snapshot_t *out);
void noah_runtime_reset_for_test(void);
