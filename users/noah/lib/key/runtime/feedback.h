// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Feedback
// ────────────────────────────────────────────────────────────────────────────
//
// Read-only snapshot of key runtime state for RGB and other feedback modules.
// Avoids exposing the rest of the key engine's private runtime surface.
//
// The full snapshot is computed on the master half. For split sync, the RGB-
// relevant semantic state is mirrored as packed flags plus one packed owner
// key so the slave can render the same feedback categories without access to
// the key engine globals. Time-based effects such as flashing may still
// compute phase locally.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

// ─── Packed flags for split sync ────────────────────────────────────────────
//
// One byte encodes the feedback state the RGB renderer needs.
// Consumers should use key_feedback_flags_*() helpers, not raw bits.

// Sequence pending: a multi-tap window is still resolving the winning tap
// index.
#define KEY_FEEDBACK_FLAG_MULTI_TAP_PENDING (1 << 0)
// Hold-tier or long-hold-tier feedback is currently active.
#define KEY_FEEDBACK_FLAG_HOLD_ACTIVE (1 << 1)
// The active feedback state belongs to the long-hold tier rather than the
// normal hold tier.
#define KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE (1 << 2)
// An authored hold tier exists on the current tap index and is pending
// resolution; long-hold-only surfaces stay quiet until the long-hold tier
// commits.
#define KEY_FEEDBACK_FLAG_HOLD_PENDING (1 << 3)
#define KEY_FEEDBACK_FLAG_LEVEL_FLASH (1 << 4)
// Current flash phase, computed on the master and synced to the slave so both
// halves flash in lockstep despite having independent clocks.
#define KEY_FEEDBACK_FLAG_FLASH_PHASE (1 << 5)

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

static inline bool key_feedback_flags_flash_phase(uint8_t flags) {
    return (flags & KEY_FEEDBACK_FLAG_FLASH_PHASE) != 0;
}

#define KEY_FEEDBACK_KEY_NONE UINT8_MAX

// Compute packed flags from the master-side key engine state.
uint8_t key_feedback_pack(void);
uint8_t key_feedback_key(void);
uint8_t key_feedback_preview_layer(void);
void    key_feedback_pulse_arm(bool long_hold_level);
