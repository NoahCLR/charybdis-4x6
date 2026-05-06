#pragma once

#include "../reducer/runtime.h"

tap_series_t *key_runtime_core_tap_series_state(key_runtime_core_state_t *state, keypos_t key_pos);
void          key_runtime_core_tap_series_clear(key_runtime_core_state_t *state, tap_series_t *series);
bool          key_runtime_core_tap_count_uses_branch_confirm(uint8_t tap_count);
bool          key_runtime_core_tap_series_has_authored_branch(const tap_series_t *series);
bool          key_runtime_core_tap_series_has_authored_tap_branch(const tap_series_t *series);
bool          key_runtime_core_tap_series_branch_confirm_window_active(uint16_t started_at, uint16_t term_ms, uint16_t now);
bool          key_runtime_core_tap_series_start_branch_confirm(key_runtime_core_state_t *state, tap_series_t *series, key_runtime_tap_series_branch_confirm_kind_t kind, uint8_t tap_count, uint16_t started_at, uint16_t term_ms);
bool          key_runtime_core_tap_series_start_delayed_action_branch_confirm(key_runtime_core_state_t *state, tap_series_t *series, uint8_t tap_count, uint16_t started_at, uint16_t term_ms, uint16_t action, uint8_t repeat_count, delayed_action_mods_t mods, bool tap_commit_feedback, bool action_feedback, key_feedback_pulse_kind_t action_feedback_kind);
bool          key_runtime_core_tap_series_pending_combo_output(const key_runtime_core_state_t *state, const tap_series_t *series);
bool          key_runtime_core_pending_multi_tap_flush_resolution(const tap_series_t *series, uint16_t *action, uint8_t *repeat_count);
bool          key_runtime_core_tap_series_take_flush(tap_series_t *series, uint16_t *action, uint8_t *repeat_count, delayed_action_mods_t *mods);
void          key_runtime_core_plan_branch_confirm_delayed_action(key_runtime_core_state_t *state, tap_series_t *series, keypos_t key_pos, key_runtime_core_effect_plan_t *plan);
void          key_runtime_core_plan_same_key_branch_confirm_interruption(key_runtime_core_state_t *state, tap_series_t *series, keypos_t key_pos, key_runtime_core_effect_plan_t *plan);
bool          key_runtime_core_reset_pending_multi_tap(keypos_t key_pos);
