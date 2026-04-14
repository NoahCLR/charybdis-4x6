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

#include "../../key/ownership/held_action.h"
#include "../../key/ownership/held_repeat.h"
#include "../ownership/keyboard_mod_ownership.h"
#include "../ownership/layer_ownership.h"
#include "runtime_shared_state.h"
#include "runtime_trace.h"

typedef struct {
    runtime_shared_state_t                  core;
    layer_ownership_debug_snapshot_t        layer_ownership;
    held_action_debug_snapshot_t            held_actions;
    held_repeat_debug_snapshot_t            held_repeats;
    keyboard_mod_ownership_debug_snapshot_t keyboard_mod_ownership;
    noah_runtime_trace_snapshot_t           trace;
} noah_runtime_debug_snapshot_t;

void noah_runtime_debug_snapshot(noah_runtime_debug_snapshot_t *out);
void noah_runtime_reset_for_test(void);
uint8_t noah_runtime_debug_active_slot_count(const noah_runtime_debug_snapshot_t *snapshot);
bool    noah_runtime_debug_active_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, uint8_t order, keypos_t *out);
uint8_t noah_runtime_debug_pending_multi_tap_slot_count(const noah_runtime_debug_snapshot_t *snapshot);
bool    noah_runtime_debug_pending_multi_tap_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, uint8_t order, keypos_t *out);
bool    noah_runtime_debug_preview_owner_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, keypos_t *out);
bool    noah_runtime_debug_pending_fallback_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, keypos_t *out);
