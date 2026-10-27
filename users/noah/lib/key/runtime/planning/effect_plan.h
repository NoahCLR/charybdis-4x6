#pragma once

#include "../reducer/runtime.h"

// A buffered plan keeps every effect for its caller, and drops what does not
// fit. A streaming plan hands its effects to transition.c as planning goes:
// when it is full, the next push first projects what it holds (auto-drain),
// and a reducer moment projects it between its steps (end_step), so each step
// plans against what the last one did. Projecting mid-planning is what lets a
// streaming plan carry any number of effects in one fixed plan.
void key_runtime_core_effect_plan_init(key_runtime_core_effect_plan_t *plan);
void key_runtime_core_effect_plan_init_streaming(key_runtime_core_effect_plan_t *plan);
void key_runtime_core_effect_plan_end_step(key_runtime_core_effect_plan_t *plan);
void key_runtime_core_effect_plan_push_dispatch_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action);
void key_runtime_core_effect_plan_push_held_action(key_runtime_core_effect_plan_t *plan, key_runtime_effect_kind_t kind, keypos_t key_pos, uint16_t action);
void key_runtime_core_effect_plan_push_release_owned_state(key_runtime_core_effect_plan_t *plan, keypos_t key_pos);
void key_runtime_core_effect_plan_push_repeat_start(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, uint16_t repeat_hz);
void key_runtime_core_effect_plan_push_layer_press(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint8_t layer);
void key_runtime_core_effect_plan_push_layer_release(key_runtime_core_effect_plan_t *plan, keypos_t key_pos);
void key_runtime_core_effect_plan_push_pointing_mode_lock_state(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, pointing_mode_mask_t mode, bool locked);
void key_runtime_core_effect_plan_push_delayed_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count);
void key_runtime_core_effect_plan_push_deferred_delayed_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count, bool tap_commit_feedback);
void key_runtime_core_effect_plan_push_feedback_pulse(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, key_feedback_pulse_kind_t kind);
void key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, uint8_t tap_count);
