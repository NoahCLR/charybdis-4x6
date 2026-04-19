// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Pending Multi-Tap
// ────────────────────────────────────────────────────────────────────────────
//
// Internal reducer helpers for pending multi-tap state. This keeps the
// multi-tap-specific release/scan policy out of the main slot reducer while
// preserving the one public `key_runtime_slot_step(...)` seam.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../../runtime_v2/runtime_v2_release_internal.h"
#include "key_runtime_slot_direct_plan.h"
#include "key_runtime_slot_result.h"
#include "../key_runtime_internal.h"

bool key_runtime_slot_take_v2_pending_multi_tap_release_plan(active_key_state_t *slot, uint16_t keycode, uint16_t elapsed, runtime_v2_release_effect_plan_t *out_plan);
key_runtime_slot_direct_plan_t key_runtime_slot_take_pending_multi_tap_scan_plan(active_key_state_t *slot);
key_runtime_slot_result_t key_runtime_slot_pending_multi_tap_handle_release(active_key_state_t *slot, uint16_t keycode, uint16_t elapsed);
key_runtime_slot_result_t key_runtime_slot_pending_multi_tap_handle_scan(active_key_state_t *slot);
