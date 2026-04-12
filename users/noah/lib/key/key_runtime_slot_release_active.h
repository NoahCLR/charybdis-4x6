// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Active Release
// ────────────────────────────────────────────────────────────────────────────
//
// Internal active-slot handled-release reducer helpers. This keeps the
// release-phase decision tree separate from the outer release ownership logic
// that still handles pending multi-tap and orphaned release cleanup.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "handled_key.h"
#include "key_runtime_slot_result.h"
#include "key_runtime_state.h"

key_runtime_slot_result_t key_runtime_slot_reduce_active_release(active_key_state_t *slot, uint16_t keycode, handled_key_view_t key);
