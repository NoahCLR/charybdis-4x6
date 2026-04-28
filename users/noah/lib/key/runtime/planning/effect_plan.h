#pragma once

#include "../reducer/runtime.h"

void key_runtime_core_effect_plan_init(key_runtime_core_effect_plan_t *plan);
void key_runtime_core_effect_plan_init_with_sink(key_runtime_core_effect_plan_t *plan, void (*sink)(void *ctx, key_runtime_effect_t effect), void *sink_ctx);
void key_runtime_core_effect_plan_push_dispatch_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action);
void key_runtime_core_effect_plan_push_held_action(key_runtime_core_effect_plan_t *plan, key_runtime_effect_kind_t kind, keypos_t key_pos, uint16_t action);
void key_runtime_core_effect_plan_push_release_owned_state(key_runtime_core_effect_plan_t *plan, keypos_t key_pos);
void key_runtime_core_effect_plan_push_repeat_start(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, uint16_t repeat_hz);
void key_runtime_core_effect_plan_push_layer_press(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint8_t layer);
void key_runtime_core_effect_plan_push_layer_release(key_runtime_core_effect_plan_t *plan, keypos_t key_pos);
void key_runtime_core_effect_plan_push_delayed_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count);
void key_runtime_core_effect_plan_push_deferred_delayed_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count, bool tap_commit_feedback);
void key_runtime_core_effect_plan_push_feedback_pulse(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, key_feedback_pulse_kind_t kind);
void key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, uint8_t tap_count);
