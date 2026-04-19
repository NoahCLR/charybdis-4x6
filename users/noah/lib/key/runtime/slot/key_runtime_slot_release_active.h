// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Active Release
// ────────────────────────────────────────────────────────────────────────────
//
// Internal active-slot handled-release reducer helpers. This keeps the
// release-phase decision tree separate from the outer release ownership logic
// that still handles pending multi-tap and orphaned release cleanup.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../interaction/handled_key.h"
#include "../../../runtime_v2/runtime_v2_release_internal.h"
#include "key_runtime_slot_result.h"
#include "../key_runtime_internal.h"

bool key_runtime_slot_take_v2_active_release_plan(active_key_state_t *slot, uint16_t keycode, runtime_v2_release_effect_plan_t *out_plan);
key_runtime_slot_result_t key_runtime_slot_reduce_active_release(active_key_state_t *slot, uint16_t keycode);
