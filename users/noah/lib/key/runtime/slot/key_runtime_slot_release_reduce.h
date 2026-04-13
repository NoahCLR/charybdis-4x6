// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Release Reducer
// ────────────────────────────────────────────────────────────────────────────
//
// Internal handled-release reducer helpers. These keep release-phase
// resolution out of the main slot-step event router while preserving the
// single public `key_runtime_slot_step(...)` seam.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../interaction/handled_key.h"
#include "key_runtime_slot_result.h"
#include "../key_runtime_state.h"

key_runtime_slot_result_t key_runtime_slot_reduce_handled_release(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution);
