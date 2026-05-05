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

// Leaf local transitions: mutate one mode and run its lifecycle side effects
// without performing any cross-mode orchestration or split-runtime sync.
void pd_mode_transition_activate(pd_mode_mask_t mode);
void pd_mode_transition_deactivate(pd_mode_mask_t mode);
void pd_mode_transition_lock(pd_mode_mask_t mode);
void pd_mode_transition_unlock(pd_mode_mask_t mode);

// Internal convenience entrypoints that route through the typed pd-mode write
// controller. These preserve the historical internal API used by host tests.
void pd_mode_activate(pd_mode_mask_t mode);
void pd_mode_deactivate(pd_mode_mask_t mode);
void pd_mode_lock(pd_mode_mask_t mode);
void pd_mode_unlock(pd_mode_mask_t mode);
