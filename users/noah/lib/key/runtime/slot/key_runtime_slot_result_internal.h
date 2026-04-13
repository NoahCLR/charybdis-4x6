// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Result Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Shared result-builder helpers used by the handled-key slot reducer and the
// focused slot helper modules.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_slot_effect.h"
#include "key_runtime_slot_result.h"

bool key_runtime_slot_result_builder_has_effect(key_runtime_effect_builder_t builder);
void key_runtime_slot_result_push(key_runtime_slot_result_t *result, key_runtime_effect_t effect);
void key_runtime_slot_result_push_builder_if_present(key_runtime_slot_result_t *result, keypos_t key_pos, key_runtime_effect_builder_t builder);
void key_runtime_slot_result_push_dispatch_action(key_runtime_slot_result_t *result, keypos_t key_pos, uint16_t action);
void key_runtime_slot_result_push_delayed_action(key_runtime_slot_result_t *result, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count);
void key_runtime_slot_result_push_layer_press(key_runtime_slot_result_t *result, keypos_t key_pos, uint8_t layer);
void key_runtime_slot_result_push_layer_release(key_runtime_slot_result_t *result, keypos_t key_pos);
void key_runtime_slot_result_push_pd_mode_lock_tap(key_runtime_slot_result_t *result, pd_mode_mask_t mode);
