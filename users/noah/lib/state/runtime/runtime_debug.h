// ────────────────────────────────────────────────────────────────────────────
// Runtime Debug
// ────────────────────────────────────────────────────────────────────────────
//
// Public key-runtime observation surface for the userspace-owned runtime state.
// Higher-level host tests can use these live semantic queries to inspect slot
// ownership, feedback, and pending multi-tap state without rebuilding a
// storage-shaped aggregate snapshot.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../key/runtime/types.h"

bool                     noah_runtime_debug_feedback_active(void);
uint16_t                 noah_runtime_debug_slot_owner_keycode(keypos_t key_pos);
uint16_t                 noah_runtime_debug_slot_tap_action(keypos_t key_pos);
uint16_t                 noah_runtime_debug_slot_held_action_keycode(keypos_t key_pos);
key_runtime_slot_phase_t noah_runtime_debug_slot_phase(keypos_t key_pos);
bool                     noah_runtime_debug_slot_momentary_tap_interrupted(keypos_t key_pos);
uint8_t                  noah_runtime_debug_slot_pending_multi_tap_count(keypos_t key_pos);
bool                     noah_runtime_debug_slot_pending_multi_tap_holding(keypos_t key_pos);
bool                     noah_runtime_debug_slot_has_pending_multi_tap(keypos_t key_pos);
bool                     noah_runtime_debug_slot_hold_is_complete(keypos_t key_pos);
uint8_t                  noah_runtime_debug_active_slot_count(void);
bool                     noah_runtime_debug_active_slot_key_pos(uint8_t order, keypos_t *out);
uint8_t                  noah_runtime_debug_pending_multi_tap_slot_count(void);
bool                     noah_runtime_debug_pending_multi_tap_slot_key_pos(uint8_t order, keypos_t *out);
uint8_t                  noah_runtime_debug_deferred_release_count(void);
bool                     noah_runtime_debug_deferred_release_key_pos(uint8_t order, keypos_t *out);
uint16_t                 noah_runtime_debug_deferred_release_action(uint8_t order);
uint8_t                  noah_runtime_debug_deferred_release_blocker_count(void);
uint8_t                  noah_runtime_debug_deferred_release_timed_blocker_count(void);
bool                     noah_runtime_debug_preview_owner_slot_key_pos(keypos_t *out);
bool                     noah_runtime_debug_pending_fallback_slot_key_pos(keypos_t *out);
