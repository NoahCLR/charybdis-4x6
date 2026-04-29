#pragma once

#include "runtime.h"

bool                     key_runtime_core_blocker_queries_authoritative(void);
bool                     key_runtime_core_has_any_deferred_release_blocker(void);
bool                     key_runtime_core_has_foreign_deferred_release_blocker_except(keypos_t key_pos);
const press_token_t     *key_runtime_core_press_token_at(keypos_t key_pos);
const tap_series_t      *key_runtime_core_tap_series_at(keypos_t key_pos);
bool                     key_runtime_core_press_token_key_pos(const press_token_t *token, keypos_t *out);
bool                     key_runtime_core_tap_series_key_pos(const tap_series_t *series, keypos_t *out);
uint8_t                  key_runtime_core_active_press_token_count(void);
bool                     key_runtime_core_active_press_token_key_pos(uint8_t order, keypos_t *out);
uint8_t                  key_runtime_core_pending_multi_tap_count(void);
bool                     key_runtime_core_pending_multi_tap_key_pos(uint8_t order, keypos_t *out);
bool                     key_runtime_core_has_other_active_press_token(keypos_t key_pos);
uint16_t                 key_runtime_core_owner_keycode_at(keypos_t key_pos);
uint16_t                 key_runtime_core_tap_action_at(keypos_t key_pos);
key_runtime_slot_phase_t key_runtime_core_slot_phase_at(keypos_t key_pos);
bool                     key_runtime_core_momentary_layer_tap_interrupted_at(keypos_t key_pos);
uint8_t                  key_runtime_core_pending_multi_tap_tap_count_at(keypos_t key_pos);
bool                     key_runtime_core_pending_multi_tap_holding_at(keypos_t key_pos);
bool                     key_runtime_core_has_pending_multi_tap_at(keypos_t key_pos);
bool                     key_runtime_core_hold_is_complete_at(keypos_t key_pos);
uint8_t                  key_runtime_core_deferred_release_blocker_count(void);
uint8_t                  key_runtime_core_deferred_release_timed_blocker_count(void);
bool                     key_runtime_core_preview_owner_key_pos(keypos_t *out);
bool                     key_runtime_core_pending_fallback_key_pos(keypos_t *out);
uint8_t                  key_runtime_core_deferred_release_blocker_count_for_keypos(keypos_t key_pos);
