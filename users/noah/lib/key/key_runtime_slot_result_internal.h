// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Result Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Shared result-builder helpers used by the handled-key slot modules. This is
// the only remaining cross-module slot result protocol.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_slot_result.h"

key_runtime_slot_result_t key_runtime_slot_take_handled_press_result(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush);
key_runtime_slot_result_t key_runtime_slot_take_handled_release_result(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, key_behavior_view_t behavior);
key_runtime_slot_result_t key_runtime_slot_take_active_scan_result(active_key_state_t *slot);
key_runtime_slot_result_t key_runtime_slot_take_pending_multi_tap_scan_result(active_key_state_t *slot);
key_runtime_slot_result_t key_runtime_slot_take_interrupt_result(active_key_state_t *slot, keypos_t other_key_pos);
key_runtime_slot_result_t key_runtime_slot_take_pending_multi_tap_flush_result(active_key_state_t *slot);

bool key_runtime_slot_result_request_has_effect(key_runtime_slot_effect_request_t request);
void key_runtime_slot_result_push(key_runtime_slot_result_t *result, key_runtime_slot_result_effect_t effect);
void key_runtime_slot_result_push_request_if_present(key_runtime_slot_result_t *result, keypos_t key_pos, key_runtime_slot_effect_request_t request);
void key_runtime_slot_result_push_dispatch_action(key_runtime_slot_result_t *result, keypos_t key_pos, uint16_t action);
void key_runtime_slot_result_push_delayed_action(key_runtime_slot_result_t *result, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count);
void key_runtime_slot_result_push_layer_press(key_runtime_slot_result_t *result, keypos_t key_pos, uint8_t layer);
void key_runtime_slot_result_push_layer_release(key_runtime_slot_result_t *result, keypos_t key_pos);
void key_runtime_slot_result_push_pd_mode_lock_tap(key_runtime_slot_result_t *result, pd_mode_mask_t mode);
