// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Effects
// ────────────────────────────────────────────────────────────────────────────
//
// Effect-request mutation helpers for handled-key slot transitions.
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_effect.h"

#include "../action/action_dispatch.h"
#include "../action/action_lifecycle.h"

typedef enum {
    KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_NONE = 0,
    KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_TAP,
    KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_HELD,
    KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_REPEAT,
} key_runtime_slot_hold_threshold_dispatch_t;

static key_runtime_slot_hold_threshold_dispatch_t key_runtime_slot_hold_threshold_dispatch_kind(hold_behavior_t hold) {
    if (!hold.present) {
        return KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_NONE;
    }

    switch (hold.mode) {
        case HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD:
            return KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_TAP;
        case HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE:
            return noah_action_hold_kind(hold.action) == NOAH_ACTION_HOLD_KIND_PRESS_ONLY ? KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_TAP : KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_HELD;
        case HOLD_BEHAVIOR_REPEAT_WHILE_HELD:
            return KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_REPEAT;
        default:
            return KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_NONE;
    }
}

static bool key_runtime_slot_hold_activation_needs_pulse(hold_behavior_t hold, bool pulse_momentary_layer_action) {
    if (action_dispatch_is_layer_lock(hold.action)) {
        return true;
    }

    if (IS_QK_MOMENTARY(hold.action)) {
        return pulse_momentary_layer_action;
    }

    return false;
}

static void key_runtime_slot_clear_owned_hold(active_key_state_t *slot, key_runtime_slot_effect_request_t *request) {
    if (!slot || !request) {
        return;
    }

    if (slot->held_action_keycode != KC_NO || slot->repeat_binding_active) {
        request->release_owned_state = true;
        slot->held_action_keycode    = KC_NO;
        slot->repeat_binding_active  = false;
    }
}

key_runtime_slot_effect_request_t key_runtime_slot_activate_pending_fallback_hold_request(active_key_state_t *slot) {
    key_runtime_slot_effect_request_t request = {0};

    if (!slot || !key_runtime_slot_uses_fallback_hold(slot) || slot->held_action_keycode != KC_NO || slot->keycode == KC_NO) {
        return request;
    }

    slot->held_action_keycode = slot->keycode;
    key_runtime_slot_commit_hold_phase(slot, true);
    request.kind              = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER;
    request.action            = slot->keycode;
    return request;
}

key_runtime_slot_effect_request_t key_runtime_slot_interrupt_on_other_press(active_key_state_t *slot, keypos_t other_key_pos) {
    if (!key_runtime_slot_active(slot) || key_runtime_keypos_equal(slot->key_pos, other_key_pos)) {
        return (key_runtime_slot_effect_request_t){0};
    }

    key_runtime_slot_effect_request_t request = key_runtime_slot_activate_pending_fallback_hold_request(slot);

    if (is_layer_key(slot->keycode)) {
        slot->layer_interrupted = true;
    }

    return request;
}

key_runtime_slot_effect_request_t key_runtime_slot_commit_immediate_hold(active_key_state_t *slot, bool needs_feedback, bool completes_hold) {
    key_runtime_slot_effect_request_t request = {0};

    if (!slot) {
        return request;
    }

    key_runtime_slot_commit_hold_phase(slot, completes_hold);

    request.feedback_pulse           = needs_feedback;
    request.feedback_long_hold_level = false;
    return request;
}

key_runtime_slot_effect_request_t key_runtime_slot_take_flush(active_key_state_t *slot, bool active_held_action_survives_flush) {
    key_runtime_slot_effect_request_t request = {0};

    if (!key_runtime_slot_active(slot)) {
        return request;
    }

    if (!key_runtime_slot_allows_tap_release(slot) || slot->held_action_keycode != KC_NO || slot->repeat_binding_active) {
        if (slot->held_action_keycode != KC_NO && !active_held_action_survives_flush) {
            request.kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER;
            request.action = slot->held_action_keycode;
        }
    } else if (!is_layer_key(slot->keycode) && slot->tap_action != KC_NO) {
        request.kind   = KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION;
        request.action = slot->tap_action;
    }

    key_runtime_slot_reset(slot);
    return request;
}

key_runtime_slot_effect_request_t key_runtime_slot_fire_hold_at_threshold(active_key_state_t *slot, hold_behavior_t hold, hold_behavior_t long_hold, bool pulse_momentary_layer_action) {
    key_runtime_slot_effect_request_t request = {0};

    if (!slot) {
        return request;
    }

    switch (key_runtime_slot_hold_threshold_dispatch_kind(hold)) {
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_slot_clear_owned_hold(slot, &request);
            request.kind                    = KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION;
            request.action                  = hold.action;
            request.feedback_pulse          = true;
            request.feedback_long_hold_level = false;
            key_runtime_slot_commit_hold_phase(slot, !long_hold.present);
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_HELD:
            slot->held_action_keycode     = hold.action;
            key_runtime_slot_commit_hold_phase(slot, !long_hold.present);
            request.kind                  = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER;
            request.action                = hold.action;
            request.feedback_pulse        = key_runtime_slot_hold_activation_needs_pulse(hold, pulse_momentary_layer_action);
            request.feedback_long_hold_level = false;
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_REPEAT:
            key_runtime_slot_clear_owned_hold(slot, &request);
            slot->repeat_binding_active   = true;
            key_runtime_slot_commit_hold_phase(slot, !long_hold.present);
            request.kind                  = KEY_RUNTIME_SLOT_EFFECT_REQUEST_REPEAT_START;
            request.action                = hold.action;
            request.repeat_hz             = hold.repeat_hz;
            request.feedback_pulse        = true;
            request.feedback_long_hold_level = false;
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_NONE:
        default:
            return request;
    }
}

key_runtime_slot_effect_request_t key_runtime_slot_promote_to_long_hold(active_key_state_t *slot, hold_behavior_t long_hold, bool pulse_momentary_layer_action) {
    key_runtime_slot_effect_request_t request = {0};

    if (!slot) {
        return request;
    }

    key_runtime_slot_clear_owned_hold(slot, &request);

    switch (key_runtime_slot_hold_threshold_dispatch_kind(long_hold)) {
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_slot_commit_hold_phase(slot, true);
            request.kind                  = KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION;
            request.action                = long_hold.action;
            request.feedback_pulse        = true;
            request.feedback_long_hold_level = true;
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_HELD:
            slot->held_action_keycode     = long_hold.action;
            key_runtime_slot_commit_hold_phase(slot, true);
            request.kind                  = KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER;
            request.action                = long_hold.action;
            request.feedback_pulse        = key_runtime_slot_hold_activation_needs_pulse(long_hold, pulse_momentary_layer_action);
            request.feedback_long_hold_level = true;
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_REPEAT:
            slot->repeat_binding_active   = true;
            key_runtime_slot_commit_hold_phase(slot, true);
            request.kind                  = KEY_RUNTIME_SLOT_EFFECT_REQUEST_REPEAT_START;
            request.action                = long_hold.action;
            request.repeat_hz             = long_hold.repeat_hz;
            request.feedback_pulse        = true;
            request.feedback_long_hold_level = true;
            return request;
        case KEY_RUNTIME_SLOT_HOLD_THRESHOLD_DISPATCH_NONE:
        default:
            return request;
    }
}
