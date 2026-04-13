// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Pending Multi-Tap
// ────────────────────────────────────────────────────────────────────────────
//
// Internal reducer helpers for pending multi-tap state. This keeps the
// multi-tap-specific release/scan policy out of the main slot reducer while
// preserving the one public `key_runtime_slot_step(...)` seam.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../interaction/handled_key.h"
#include "key_runtime_slot_result.h"
#include "../key_runtime_state.h"

key_runtime_slot_result_t key_runtime_slot_pending_multi_tap_handle_release(active_key_state_t *slot, uint16_t keycode, handled_key_view_t key, uint16_t elapsed);
key_runtime_slot_result_t key_runtime_slot_pending_multi_tap_handle_scan(active_key_state_t *slot);
