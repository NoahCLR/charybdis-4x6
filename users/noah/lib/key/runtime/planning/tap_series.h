#pragma once

#include "../reducer/runtime.h"

tap_series_t *key_runtime_core_tap_series_state(key_runtime_core_state_t *state, keypos_t key_pos);
void          key_runtime_core_tap_series_clear(key_runtime_core_state_t *state, tap_series_t *series);
bool          key_runtime_core_tap_series_has_authored_branch(const tap_series_t *series);
bool          key_runtime_core_tap_series_has_authored_tap_branch(const tap_series_t *series);
bool          key_runtime_core_tap_series_pending_combo_output(const key_runtime_core_state_t *state, const tap_series_t *series);
bool          key_runtime_core_pending_multi_tap_flush_resolution(const tap_series_t *series, uint16_t *action, uint8_t *repeat_count);
bool          key_runtime_core_tap_series_take_flush(tap_series_t *series, uint16_t *action, uint8_t *repeat_count, delayed_action_mods_t *mods);
bool          key_runtime_core_reset_pending_multi_tap(keypos_t key_pos);
