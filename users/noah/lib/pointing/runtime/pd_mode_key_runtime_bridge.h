#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../defs/pd_mode_flags.h"

// PD runtime owns the actual local PD lock state. Key runtime observes changed
// local lock state here to keep its shadow projection coherent.
void pd_mode_key_runtime_bridge_observe_local_lock_state(pd_mode_mask_t mode, bool locked);
