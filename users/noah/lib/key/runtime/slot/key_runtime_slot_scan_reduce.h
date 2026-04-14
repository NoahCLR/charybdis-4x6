// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Scan Reducer
// ────────────────────────────────────────────────────────────────────────────
//
// Internal active-scan reducer helpers. This keeps phase-local scan handling
// out of the main slot-step event router while preserving the public
// `key_runtime_slot_step(...)` seam.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_runtime_slot_result.h"
#include "../key_runtime_internal.h"

key_runtime_slot_result_t key_runtime_slot_reduce_active_scan(active_key_state_t *slot);
