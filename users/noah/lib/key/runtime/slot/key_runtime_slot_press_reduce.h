// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Press Reducer
// ────────────────────────────────────────────────────────────────────────────
//
// Internal handled-press reducer helpers. This keeps press-phase setup and
// pending-multi-tap reuse/flush policy out of the main slot-step event router
// while preserving the single public `key_runtime_slot_step(...)` seam.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../interaction/handled_key.h"
#include "key_runtime_slot_result.h"
#include "../key_runtime_state.h"

key_runtime_slot_result_t key_runtime_slot_reduce_handled_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution, bool active_held_action_survives_flush);
