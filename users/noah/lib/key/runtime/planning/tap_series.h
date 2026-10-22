#pragma once

#include "../reducer/runtime.h"

tap_series_t *key_runtime_core_tap_series_state(key_runtime_core_state_t *state, keypos_t key_pos);
void          key_runtime_core_tap_series_clear(key_runtime_core_state_t *state, tap_series_t *series);
bool          key_runtime_core_tap_series_has_authored_branch(const tap_series_t *series);
bool          key_runtime_core_tap_series_has_authored_tap_branch(const tap_series_t *series);
bool          key_runtime_core_tap_series_pending_gesture(const key_runtime_core_state_t *state, const tap_series_t *series);
bool          key_runtime_core_pending_multi_tap_flush_resolution(const tap_series_t *series, uint16_t *action, uint8_t *repeat_count);
bool          key_runtime_core_tap_series_take_flush(tap_series_t *series, uint16_t *action, uint8_t *repeat_count, delayed_action_mods_t *mods);
bool          key_runtime_core_reset_pending_multi_tap(keypos_t key_pos);

// A series whose key is up, or whose hold tier has taken the key over, holds
// nothing.
static inline void key_runtime_core_tap_series_drop_hold(tap_series_t *series) {
    series->pending_hold = false;
    series->hold         = hold_behavior_none();
    series->long_hold    = hold_behavior_none();
}

// The key's hold that waits for its release has been reached: the series has
// entered its branch and accepts no further taps.
static inline void key_runtime_core_tap_series_resolve(tap_series_t *series) {
    series->resolved = true;
    key_runtime_core_tap_series_drop_hold(series);
}
