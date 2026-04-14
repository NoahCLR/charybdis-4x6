// ────────────────────────────────────────────────────────────────────────────
// Runtime Debug Snapshot
// ────────────────────────────────────────────────────────────────────────────
//
// Public key-runtime observation surface for the userspace-owned runtime state.
// Higher-level host tests can use this to inspect slot ownership, feedback,
// and pending multi-tap state without reaching into the runtime-owned storage
// layout directly.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

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
} noah_runtime_debug_snapshot_t;

void noah_runtime_debug_snapshot(noah_runtime_debug_snapshot_t *out);
bool noah_runtime_debug_feedback_active(const noah_runtime_debug_snapshot_t *snapshot);
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
