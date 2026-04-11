// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Admission
// ────────────────────────────────────────────────────────────────────────────
//
// Slot lookup and admission policy for handled-key presses. This keeps slot
// capacity and reclaim decisions separate from slot lifecycle mutations.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_state.h"

active_key_state_t *key_runtime_first_active_slot(void);
active_key_state_t *key_runtime_find_slot_by_position(keypos_t key_pos);
active_key_state_t *key_runtime_find_slot_with_pending_multi_tap(keypos_t key_pos);
active_key_state_t *key_runtime_find_free_slot(void);
active_key_state_t *key_runtime_find_reclaimable_slot(void);
active_key_state_t *key_runtime_select_slot_for_press(keypos_t key_pos);
bool                active_key_matches(uint16_t keycode, keypos_t key_pos);
