// ────────────────────────────────────────────────────────────────────────────
// Action Kind Dispatch Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal dispatch hooks for executing action semantics once an authored
// action has already been classified.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "action_dispatch.h"

void noah_action_desc_tap_dispatch(noah_action_desc_t desc);
void noah_action_desc_press_dispatch(noah_action_desc_t desc, keypos_t key_pos);
void noah_action_desc_release_dispatch(noah_action_desc_t desc, keypos_t key_pos);
