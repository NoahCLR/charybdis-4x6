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
#include "../../pointing/defs/pd_mode_flags.h"
#include "../ownership/keyboard_mod_ownership.h"
#include "../ownership/layer_ownership.h"
#include "runtime_trace.h"

#define NOAH_RUNTIME_DEBUG_SLOT_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))

typedef struct {
    uint16_t owner_keycode;
    uint16_t tap_action;
    uint16_t held_action_keycode;
    uint8_t  pending_multi_tap_count;
    bool     pending_multi_tap_holding;
    bool     has_pending_multi_tap;
    bool     hold_complete;
} noah_runtime_debug_slot_snapshot_t;

typedef struct {
    bool                               feedback_active;
    noah_runtime_debug_slot_snapshot_t slots_by_position[NOAH_RUNTIME_DEBUG_SLOT_CAPACITY];
    keypos_t                           active_slots[NOAH_RUNTIME_DEBUG_SLOT_CAPACITY];
    uint8_t                            active_slot_count;
    keypos_t                           pending_multi_tap_slots[NOAH_RUNTIME_DEBUG_SLOT_CAPACITY];
    uint8_t                            pending_multi_tap_count;
    keypos_t                           preview_owner_slot;
    bool                               preview_owner_slot_present;
    keypos_t                           pending_fallback_slot;
    bool                               pending_fallback_slot_present;
} noah_runtime_debug_key_snapshot_t;

typedef struct {
    pd_mode_mask_t local_active_mode;
    pd_mode_mask_t local_locked_mode;
    pd_mode_mask_t remote_display_active_mode;
    pd_mode_mask_t remote_display_locked_mode;
} noah_runtime_debug_pd_snapshot_t;

typedef struct {
    noah_runtime_debug_key_snapshot_t       key;
    noah_runtime_debug_pd_snapshot_t        pd;
    layer_ownership_debug_snapshot_t        layer_ownership;
    held_action_debug_snapshot_t            held_actions;
    held_repeat_debug_snapshot_t            held_repeats;
    keyboard_mod_ownership_debug_snapshot_t keyboard_mod_ownership;
    noah_runtime_trace_snapshot_t           trace;
} noah_runtime_debug_snapshot_t;

void noah_runtime_debug_snapshot(noah_runtime_debug_snapshot_t *out);
void noah_runtime_reset_for_test(void);
uint16_t noah_runtime_debug_slot_owner_keycode(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
uint16_t noah_runtime_debug_slot_tap_action(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
uint16_t noah_runtime_debug_slot_held_action_keycode(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
uint8_t  noah_runtime_debug_slot_pending_multi_tap_count(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
bool     noah_runtime_debug_slot_pending_multi_tap_holding(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
bool     noah_runtime_debug_slot_has_pending_multi_tap(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
bool     noah_runtime_debug_slot_hold_is_complete(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
uint8_t noah_runtime_debug_active_slot_count(const noah_runtime_debug_snapshot_t *snapshot);
bool    noah_runtime_debug_active_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, uint8_t order, keypos_t *out);
uint8_t noah_runtime_debug_pending_multi_tap_slot_count(const noah_runtime_debug_snapshot_t *snapshot);
bool    noah_runtime_debug_pending_multi_tap_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, uint8_t order, keypos_t *out);
bool    noah_runtime_debug_preview_owner_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, keypos_t *out);
bool    noah_runtime_debug_pending_fallback_slot_key_pos(const noah_runtime_debug_snapshot_t *snapshot, keypos_t *out);
