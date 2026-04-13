// ────────────────────────────────────────────────────────────────────────────
// PD Mode Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal state mutation and side-effectful transitions for the pd-mode
// runtime. Keep these out of the public header so non-pointing modules cannot
// bypass the state machine invariants.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../defs/pd_modes.h"

void pd_mode_set(pd_mode_mask_t mode);
void pd_mode_clear(pd_mode_mask_t mode);
void pd_mode_set_locked(pd_mode_mask_t mode);
void pd_mode_clear_locked(pd_mode_mask_t mode);

void pd_mode_activate(pd_mode_mask_t mode);
void pd_mode_deactivate(pd_mode_mask_t mode);
void pd_mode_lock(pd_mode_mask_t mode);
void pd_mode_unlock(pd_mode_mask_t mode);

bool pd_mode_unlock_other_locks(pd_mode_mask_t keep_mode);
bool pd_mode_deactivate_other_unlocked(pd_mode_mask_t keep_mode);
void pd_mode_update(pd_mode_mask_t mode, bool active);
