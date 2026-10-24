#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../defs/pointing_mode_flags.h"

// PD runtime owns the actual local PD lock state. Key runtime observes changed
// local lock state here to keep its shadow projection coherent.
void pointing_mode_key_runtime_bridge_observe_local_lock_state(pointing_mode_mask_t mode, bool locked);
