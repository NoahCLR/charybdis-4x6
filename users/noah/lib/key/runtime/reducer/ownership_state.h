#pragma once

#include "runtime.h"

bool                                        key_runtime_core_keypos_valid(keypos_t key_pos);
bool                                        key_runtime_core_owner_has_lease_kind(const key_runtime_core_state_t *state, uint16_t owner_token_id, lease_kind_t kind);
bool                                        key_runtime_core_owner_has_runtime_owned_state_lease(const key_runtime_core_state_t *state, uint16_t owner_token_id);
void                                        key_runtime_core_release_leases_for_token(key_runtime_core_state_t *state, uint16_t owner_token_id);
void                                        key_runtime_core_adopt_runtime_owned_state_leases(key_runtime_core_state_t *state, uint16_t from_owner_token_id, uint16_t to_owner_token_id);
void                                        key_runtime_core_shadow_projection_recompute(key_runtime_core_state_t *state);
void                                        key_runtime_core_press_token_attach_press_leases(key_runtime_core_state_t *state, const press_token_t *token);
void                                        key_runtime_core_press_token_attach_hold_leases(key_runtime_core_state_t *state, const press_token_t *token);
uint16_t                                    key_runtime_core_key_pos_held_action_keycode(const key_runtime_core_state_t *state, keypos_t key_pos);
bool                                        key_runtime_core_key_pos_repeat_active(const key_runtime_core_state_t *state, keypos_t key_pos);
void                                        key_runtime_core_observe_held_action_register(keypos_t key_pos, uint16_t action);
void                                        key_runtime_core_observe_held_action_unregister(keypos_t key_pos, uint16_t action);
void                                        key_runtime_core_observe_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz);
bool                                        key_runtime_core_release_owned_state_by_key(keypos_t key_pos);
bool                                        key_runtime_core_finalize_non_handled_release(keypos_t key_pos);
uint16_t                                    key_runtime_core_held_action_keycode_at(keypos_t key_pos);
bool                                        key_runtime_core_repeat_active_at(keypos_t key_pos);
bool                                        key_runtime_core_flashing_feedback_visible_at(keypos_t key_pos);
bool                                        key_runtime_core_flashing_feedback_sequence_at(keypos_t key_pos, uint32_t *out_sequence);
const key_runtime_core_shadow_projection_t *key_runtime_core_shadow_projection(void);
void                                        key_runtime_core_layer_lock_set(uint8_t layer, bool active);
void                                        key_runtime_core_observe_pd_mode_lock_state(pd_mode_mask_t mode, bool active);
