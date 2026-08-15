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

#ifdef PD_MODE_PUBLISH_TEST_BACKEND
// Host-test seam. The registered hook runs after the mirrored mode identity
// has been stored into the pending slot but before that slot is published, so
// a test can interleave a main-context display read into the middle of one
// remote snapshot deterministically.
typedef void (*pd_mode_publish_seam_fn_t)(void);

void pd_mode_test_set_publish_seam(pd_mode_publish_seam_fn_t seam);
#endif
