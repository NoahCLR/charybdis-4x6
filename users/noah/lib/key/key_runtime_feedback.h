// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Feedback
// ────────────────────────────────────────────────────────────────────────────
//
// Read-only snapshot of key runtime state for RGB and other feedback modules.
// Avoids exposing key_runtime_internal.h outside the key engine.
//
// The full snapshot is computed on the master half. For split sync, the RGB-
// relevant semantic state is packed into a single flags byte so the slave can
// render the same feedback categories without access to the key engine
// globals. Time-based effects such as flashing may still compute phase
// locally.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

// ─── Packed flags for split sync ────────────────────────────────────────────
//
// One byte encodes the feedback state the RGB renderer needs.
// Consumers should use key_feedback_flags_*() helpers, not raw bits.

#define KEY_FEEDBACK_FLAG_MULTI_TAP_PENDING (1 << 0)
#define KEY_FEEDBACK_FLAG_HOLD_ACTIVE       (1 << 1)
#define KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE  (1 << 2)
#define KEY_FEEDBACK_FLAG_HOLD_PENDING      (1 << 3)
#define KEY_FEEDBACK_FLAG_LEVEL_FLASH       (1 << 4)

static inline bool key_feedback_flags_multi_tap_pending(uint8_t flags) {
    return (flags & KEY_FEEDBACK_FLAG_MULTI_TAP_PENDING) != 0;
}

static inline bool key_feedback_flags_hold_active(uint8_t flags) {
    return (flags & KEY_FEEDBACK_FLAG_HOLD_ACTIVE) != 0;
}

static inline bool key_feedback_flags_long_hold_active(uint8_t flags) {
    return (flags & KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE) != 0;
}

static inline bool key_feedback_flags_hold_pending(uint8_t flags) {
    return (flags & KEY_FEEDBACK_FLAG_HOLD_PENDING) != 0;
}

static inline bool key_feedback_flags_level_flash(uint8_t flags) {
    return (flags & KEY_FEEDBACK_FLAG_LEVEL_FLASH) != 0;
}

// Compute packed flags from the master-side key engine state.
uint8_t key_feedback_pack(void);
