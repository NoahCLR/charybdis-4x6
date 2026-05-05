#pragma once

#include "../reducer/runtime.h"

void key_runtime_core_pd_projection_preempt_held_action(uint16_t action);
void key_runtime_core_pd_projection_project_lock_tap(pd_mode_mask_t mode, keypos_t key_pos);
void key_runtime_core_pd_projection_project_lock_state(pd_mode_mask_t mode, bool locked, keypos_t key_pos);
