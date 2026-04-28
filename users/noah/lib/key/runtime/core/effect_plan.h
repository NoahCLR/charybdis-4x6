#pragma once

#include "runtime.h"

void key_runtime_core_effect_plan_push_delayed_action(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, delayed_action_mods_t mods, uint8_t repeat_count);
void key_runtime_core_effect_plan_push_feedback_pulse(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, key_feedback_pulse_kind_t kind);
void key_runtime_core_effect_plan_push_tap_commit_feedback_pulse(key_runtime_core_effect_plan_t *plan, keypos_t key_pos, uint16_t action, uint8_t tap_count);
