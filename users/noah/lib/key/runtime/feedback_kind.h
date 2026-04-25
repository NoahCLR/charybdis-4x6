// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Feedback Kinds
// ────────────────────────────────────────────────────────────────────────────
#pragma once

typedef enum {
    KEY_FEEDBACK_PULSE_HOLD = 0,
    KEY_FEEDBACK_PULSE_LONG_HOLD,
    KEY_FEEDBACK_PULSE_TAP_COMMITTED,
} key_feedback_pulse_kind_t;

typedef enum {
    KEY_FEEDBACK_TAP_COMMIT_OFF = 0,
    KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS,
    KEY_FEEDBACK_TAP_COMMIT_ALL_TAPS,
} key_feedback_tap_commit_mode_t;

typedef enum {
    KEY_FEEDBACK_TAP_PENDING_SINGLE_COLOR = 0,
    KEY_FEEDBACK_TAP_PENDING_BRANCH_COLORS,
} key_feedback_tap_pending_mode_t;

key_feedback_tap_commit_mode_t key_feedback_tap_commit_mode(void);
