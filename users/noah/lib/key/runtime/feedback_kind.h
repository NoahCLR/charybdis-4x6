// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Feedback Kinds
// ────────────────────────────────────────────────────────────────────────────
#pragma once

typedef enum {
    KEY_FEEDBACK_PULSE_HOLD = 0,
    KEY_FEEDBACK_PULSE_LONG_HOLD,
    KEY_FEEDBACK_PULSE_TAP_COMMITTED,
} key_feedback_pulse_kind_t;

// Gates the branch-confirm window, which delays a non-base branch's action so its
// color is seen before it fires. The tap-phase display does not consult this: it
// shows the selected branch until that branch is entered, whether or not a window
// is holding the action back.
typedef enum {
    KEY_FEEDBACK_BRANCH_CONFIRM_OFF = 0,
    KEY_FEEDBACK_BRANCH_CONFIRM_NON_BASE_TAPS,
} key_feedback_branch_confirm_mode_t;

typedef enum {
    KEY_FEEDBACK_TAP_COMMIT_OFF = 0,
    KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS,
} key_feedback_tap_commit_mode_t;

key_feedback_branch_confirm_mode_t key_feedback_branch_confirm_mode(void);
key_feedback_tap_commit_mode_t     key_feedback_tap_commit_mode(void);
