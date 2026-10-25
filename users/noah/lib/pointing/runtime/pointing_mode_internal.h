// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Internal state mutation and side-effectful transitions for the pointing-mode
// runtime. Keep these out of the public header so non-pointing modules cannot
// bypass the state machine invariants.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../defs/pointing_modes.h"

// Only pointing_mode_apply_command's transitions call these: each change of the
// active mode there also clears the mode's owner list, whose entries record no
// mode of their own (pointing_mode_state.c).
void pointing_mode_set(pointing_mode_mask_t mode);
void pointing_mode_clear(pointing_mode_mask_t mode);
void pointing_mode_set_locked(pointing_mode_mask_t mode);
void pointing_mode_clear_locked(pointing_mode_mask_t mode);

// Leaf local transitions: mutate one mode and run its lifecycle side effects
// without performing any cross-mode orchestration or split-runtime sync.
void pointing_mode_transition_activate(pointing_mode_mask_t mode);
void pointing_mode_transition_activate_from(pointing_mode_mask_t mode, pointing_mode_source_t source);
// A key holds or locks the mode its layer runs: the key becomes its source,
// with a key-started mode's auto-mouse side effects (D-F62).
void pointing_mode_transition_take_from_layer(pointing_mode_mask_t mode);
// The key that ran the mode let it go while its linked layer still starts it:
// it keeps running, unlocked, with its layer as its source (D-F62).
void pointing_mode_transition_hand_to_layer(pointing_mode_mask_t mode);
void pointing_mode_transition_deactivate(pointing_mode_mask_t mode);
void pointing_mode_transition_lock(pointing_mode_mask_t mode);
void pointing_mode_transition_unlock(pointing_mode_mask_t mode);

// Internal convenience entrypoints that route through the typed pointing-mode
// write controller. These preserve the historical internal API used by host
// tests.
void pointing_mode_activate(pointing_mode_mask_t mode);
void pointing_mode_deactivate(pointing_mode_mask_t mode);
void pointing_mode_lock(pointing_mode_mask_t mode);
void pointing_mode_unlock(pointing_mode_mask_t mode);

#ifdef POINTING_MODE_PUBLISH_TEST_BACKEND
// Host-test seam. The registered hook runs after the mirrored mode identity
// has been stored into the pending slot but before that slot is published, so
// a test can interleave a main-context display read into the middle of one
// remote snapshot deterministically.
typedef void (*pointing_mode_publish_seam_fn_t)(void);

void pointing_mode_test_set_publish_seam(pointing_mode_publish_seam_fn_t seam);
#endif
