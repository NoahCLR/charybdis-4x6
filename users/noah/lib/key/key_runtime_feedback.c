// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Feedback
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_internal.h"
#include "key_runtime_feedback.h"

typedef struct {
    uint16_t timer;
    bool     active;
    bool     long_hold_level;
} key_feedback_pulse_state_t;

static key_feedback_pulse_state_t key_feedback_pulse = {0};

static bool key_feedback_pulse_active(void) {
    if (!key_feedback_pulse.active) {
        return false;
    }

    if (timer_elapsed(key_feedback_pulse.timer) < RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS) {
        return true;
    }

    key_feedback_pulse.active = false;
    return false;
}

void key_feedback_pulse_arm(bool long_hold_level) {
    key_feedback_pulse = (key_feedback_pulse_state_t){
        .timer           = timer_read(),
        .active          = true,
        .long_hold_level = long_hold_level,
    };
}

uint8_t key_feedback_pack(void) {
    uint8_t flags = 0;

    if (key_feedback_pulse_active()) {
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        if (key_feedback_pulse.long_hold_level) {
            flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        }
        return flags;
    }

    // Multi-tap pending: window is open, not yet in pending-hold state.
    if (multi_tap_active(&multi_tap) && !multi_tap_pending_hold(&multi_tap)) {
        flags |= KEY_FEEDBACK_FLAG_MULTI_TAP_PENDING;
    }

    bool ak_active = active_key.keycode != KC_NO;
    if (!ak_active) return flags;

    uint16_t elapsed           = timer_elapsed(active_key.timer);
    bool     long_hold_reached = active_key.long_hold.present && elapsed >= active_key.longer_hold_term;

    if (active_key.held_action_keycode != KC_NO) {
        // Held layer actions do not keep a hold overlay once the layer is on;
        // the layer switch itself is the feedback.
        if (action_dispatch_is_layer_action(active_key.held_action_keycode)) {
            return flags;
        }

        // PRESS_AND_HOLD_UNTIL_RELEASE stays visibly active while registered.
        flags |= KEY_FEEDBACK_FLAG_LEVEL_FLASH;
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        if (long_hold_reached) {
            flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        }
        return flags;
    }

    if (long_hold_reached && hold_sends_on_release(active_key.long_hold)) {
        // TAP_ON_RELEASE_AFTER_HOLD keeps feedback visible because the action
        // is still pending until release.
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        return flags;
    }

    if (!long_hold_reached && elapsed >= active_key.tap_hold_term && hold_sends_on_release(active_key.hold)) {
        flags |= KEY_FEEDBACK_FLAG_HOLD_PENDING;
        return flags;
    }

    // One-shot threshold actions are complete as soon as they fire, so they do
    // not keep a hold color latched after the threshold. Only unresolved
    // between-threshold states keep the pending color.
    if (!active_key.hold_fired && !active_key.hold_one_shot_fired && elapsed >= active_key.tap_hold_term && (active_key.hold.present || active_key.long_hold.present)) {
        flags |= KEY_FEEDBACK_FLAG_HOLD_PENDING;
    }

    return flags;
}
