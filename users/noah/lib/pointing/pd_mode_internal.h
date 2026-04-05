// ────────────────────────────────────────────────────────────────────────────
// PD Mode Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal state mutation and side-effectful transitions for the pd-mode
// runtime. Keep these out of the public header so non-pointing modules cannot
// bypass the state machine invariants.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "pd_modes.h"

void pd_mode_set(uint8_t mode);
void pd_mode_clear(uint8_t mode);
void pd_mode_set_locked(uint8_t mode);
void pd_mode_clear_locked(uint8_t mode);

void pd_mode_activate(uint8_t mode);
void pd_mode_deactivate(uint8_t mode);
void pd_mode_lock(uint8_t mode);
void pd_mode_unlock(uint8_t mode);

bool pd_mode_unlock_other_locks(uint8_t keep_mode);
bool pd_mode_deactivate_other_unlocked(uint8_t keep_mode);
void pd_mode_update(uint8_t mode, bool active);
