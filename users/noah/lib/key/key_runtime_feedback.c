// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Feedback
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_state.h"
#include "key_runtime_feedback.h"
#include "../action/action_dispatch.h"
#include "../pointing/pd_modes.h"

#define key_feedback_pulse (noah_runtime_shared_state.key.feedback)

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS
#    define KEY_FEEDBACK_FLASH_HALF_PERIOD_MS RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS
#else
#    define KEY_FEEDBACK_FLASH_HALF_PERIOD_MS 200
#endif

static bool key_feedback_pulse_active(void) {
    if (!key_feedback_pulse.active) {
        return false;
    }

    if (timer_elapsed(key_feedback_pulse.timer) < KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) {
        return true;
    }

    key_feedback_pulse.active = false;
    return false;
}

void key_feedback_pulse_arm(bool long_hold_level) {
    key_feedback_pulse = (key_runtime_feedback_state_t){
        .timer           = timer_read(),
        .active          = true,
        .long_hold_level = long_hold_level,
    };
}

static uint8_t key_feedback_layer_hint_from_action(uint16_t action) {
    if (!IS_QK_MOMENTARY(action)) {
        return UINT8_MAX;
    }

    return QK_MOMENTARY_GET_LAYER(action);
}

uint8_t key_feedback_preview_layer(void) {
    if (active_key.keycode == KC_NO || active_key.implicit_hold || active_key.fallback_hold_pending) {
        return UINT8_MAX;
    }

    if (active_key.held_action_keycode != KC_NO) {
        return key_feedback_layer_hint_from_action(active_key.held_action_keycode);
    }

    if (active_key.hold_fired || active_key.hold_one_shot_fired) {
        return UINT8_MAX;
    }

    if (!active_key.hold.present || active_key.hold.mode != HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE) {
        return UINT8_MAX;
    }

    return key_feedback_layer_hint_from_action(active_key.hold.action);
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

    if (active_key.implicit_hold) {
        return flags;
    }

    // Fallback base holds are internal runtime glue for "tap override, normal
    // hold" semantics. They are not authored hold surfaces, so keep RGB quiet.
    if (active_key.fallback_hold_pending) {
        return flags;
    }

    if (active_key.held_action_keycode != KC_NO) {
        // Held layer and pd-mode actions do not keep a hold overlay once they
        // are active; the layer or pd-mode color itself is the feedback.
        if (action_dispatch_is_layer_action(active_key.held_action_keycode) || pd_mode_for_keycode(active_key.held_action_keycode)) {
            return flags;
        }

        // PRESS_AND_HOLD_UNTIL_RELEASE stays visibly active while registered.
        // Pack the current flash phase so both halves flash in lockstep — the
        // slave reads this bit from the sync packet instead of computing phase
        // from its own independent clock.
        flags |= KEY_FEEDBACK_FLAG_LEVEL_FLASH;
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        if (((timer_read() / KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) & 1u) == 0) {
            flags |= KEY_FEEDBACK_FLAG_FLASH_PHASE;
        }
        if (long_hold_reached) {
            flags |= KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE;
        }
        return flags;
    }

    if (active_key.repeat_binding_active) {
        flags |= KEY_FEEDBACK_FLAG_LEVEL_FLASH;
        flags |= KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
        if (((timer_read() / KEY_FEEDBACK_FLASH_HALF_PERIOD_MS) & 1u) == 0) {
            flags |= KEY_FEEDBACK_FLAG_FLASH_PHASE;
        }
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
    // not keep a hold color latched after the threshold. Only an authored
    // normal hold tier keeps the pending hold color before it resolves;
    // long-hold-only surfaces stay quiet until the long-hold tier commits.
    if (!active_key.hold_fired && !active_key.hold_one_shot_fired && elapsed >= active_key.tap_hold_term && active_key.hold.present) {
        flags |= KEY_FEEDBACK_FLAG_HOLD_PENDING;
    }

    return flags;
}

#undef key_feedback_pulse
#undef KEY_FEEDBACK_FLASH_HALF_PERIOD_MS
