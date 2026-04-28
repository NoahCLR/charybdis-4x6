#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../defs/pd_mode_flags.h"

void pd_mode_key_runtime_bridge_observe_lock_state(pd_mode_mask_t mode, bool locked);
