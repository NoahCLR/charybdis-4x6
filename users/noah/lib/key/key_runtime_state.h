// ────────────────────────────────────────────────────────────────────────────
// Key Runtime State
// ────────────────────────────────────────────────────────────────────────────
//
// Shared active-slot and multi-tap state for the split key runtime modules.
// The helper surface keeps higher-level code off the raw storage layout so the
// runtime can grow toward richer per-key state without another global-state
// sweep.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../state/runtime_shared_state.h"

// Transitional aliases while key-runtime modules move from file-local globals
// to the shared runtime-owned state object.
#define active_key (noah_runtime_shared_state.key.active_slots[0])
#define multi_tap (noah_runtime_shared_state.key.multi_tap_slots[0])

void noah_key_runtime_scan(void);

uint8_t             behavior_get_layer(uint16_t keycode);
bool                is_layer_key(uint16_t keycode);
bool                key_runtime_keypos_equal(keypos_t lhs, keypos_t rhs);
active_key_state_t *key_runtime_primary_slot(void);
active_key_state_t *key_runtime_slot_at(uint8_t index);
active_key_state_t *key_runtime_first_active_slot(void);
uint8_t             key_runtime_slot_index(const active_key_state_t *slot);
bool                key_runtime_slot_active(const active_key_state_t *slot);
bool                key_runtime_slot_matches(const active_key_state_t *slot, uint16_t keycode, keypos_t key_pos);
active_key_state_t *key_runtime_find_slot_by_position(keypos_t key_pos);
active_key_state_t *key_runtime_find_slot_with_pending_multi_tap(keypos_t key_pos);
active_key_state_t *key_runtime_find_free_slot(void);
active_key_state_t *key_runtime_find_reclaimable_slot(void);
active_key_state_t *key_runtime_select_slot_for_press(keypos_t key_pos);
multi_tap_t        *key_runtime_primary_multi_tap(void);
multi_tap_t        *key_runtime_multi_tap_slot_at(uint8_t index);
multi_tap_t        *key_runtime_multi_tap_for_slot(const active_key_state_t *slot);
active_key_state_t *key_runtime_slot_for_multi_tap(const multi_tap_t *mt);
bool                key_runtime_multi_tap_slot_active(const multi_tap_t *mt);
multi_tap_t        *key_runtime_first_active_multi_tap(void);
multi_tap_t        *key_runtime_find_multi_tap_by_position(keypos_t key_pos);
bool                active_key_matches(uint16_t keycode, keypos_t key_pos);
bool                key_runtime_slot_activate_pending_fallback_hold(active_key_state_t *slot);
bool                key_runtime_activate_pending_fallback_hold(void);

void key_runtime_slot_reset(active_key_state_t *slot);
void key_runtime_slot_track(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired);
void active_key_reset(void);
void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired);
