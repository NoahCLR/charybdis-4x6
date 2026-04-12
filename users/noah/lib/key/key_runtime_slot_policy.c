// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Policy
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_policy.h"

#include "../action/action_dispatch.h"
#include "../action/action_lifecycle.h"

typedef enum {
    KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_NONE = 0,
    KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_TAP,
    KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_HELD,
    KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_REPEAT,
} key_runtime_slot_policy_hold_threshold_dispatch_t;

static key_runtime_slot_policy_hold_threshold_dispatch_t key_runtime_slot_policy_hold_threshold_dispatch_kind(hold_behavior_t hold) {
    if (!hold.present) {
        return KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_NONE;
    }

    switch (hold.mode) {
        case HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD:
            return KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_TAP;
        case HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE:
            return noah_action_hold_kind(hold.action) == NOAH_ACTION_HOLD_KIND_PRESS_ONLY ? KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_TAP : KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_HELD;
        case HOLD_BEHAVIOR_REPEAT_WHILE_HELD:
            return KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_REPEAT;
        default:
            return KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_NONE;
    }
}

static bool key_runtime_slot_policy_hold_activation_needs_pulse(hold_behavior_t hold, bool pulse_momentary_layer_action) {
    if (action_dispatch_is_layer_lock(hold.action)) {
        return true;
    }

    if (IS_QK_MOMENTARY(hold.action)) {
        return pulse_momentary_layer_action;
    }

    return false;
}

static void key_runtime_slot_policy_clear_owned_hold(active_key_state_t *slot, key_runtime_effect_builder_t *builder) {
    if (!slot || !builder) {
        return;
    }

    if (slot->lifecycle.held_action_keycode != KC_NO || slot->lifecycle.repeat_binding_active) {
        builder->release_owned_state          = true;
        slot->lifecycle.held_action_keycode   = KC_NO;
        slot->lifecycle.repeat_binding_active = false;
    }
}

uint16_t key_runtime_slot_policy_select_release_hold_action(uint16_t elapsed, uint16_t hold_action, hold_behavior_t long_hold, uint16_t longer_hold_term) {
    if (hold_sends_on_release(long_hold) && elapsed >= longer_hold_term) {
        return long_hold.action;
    }

    return hold_action;
}

key_runtime_effect_builder_t key_runtime_slot_policy_activate_pending_fallback_hold(active_key_state_t *slot) {
    key_runtime_effect_builder_t builder = {0};

    if (!slot || !key_runtime_slot_uses_fallback_hold(slot) || slot->lifecycle.held_action_keycode != KC_NO || slot->owner.keycode == KC_NO) {
        return builder;
    }

    slot->lifecycle.held_action_keycode = slot->owner.keycode;
    key_runtime_slot_commit_hold_phase(slot, true);
    builder.kind   = KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER;
    builder.action = slot->owner.keycode;
    return builder;
}

key_runtime_effect_builder_t key_runtime_slot_policy_interrupt_on_other_press(active_key_state_t *slot, keypos_t other_key_pos) {
    if (!key_runtime_slot_active(slot) || key_runtime_keypos_equal(slot->owner.key_pos, other_key_pos)) {
        return (key_runtime_effect_builder_t){0};
    }

    key_runtime_effect_builder_t builder = key_runtime_slot_policy_activate_pending_fallback_hold(slot);

    if (is_layer_key(slot->owner.keycode)) {
        slot->lifecycle.layer_interrupted = true;
    }

    return builder;
}

key_runtime_effect_builder_t key_runtime_slot_policy_commit_immediate_hold(active_key_state_t *slot, bool needs_feedback, bool completes_hold) {
    key_runtime_effect_builder_t builder = {0};

    if (!slot) {
        return builder;
    }

    key_runtime_slot_commit_hold_phase(slot, completes_hold);
    builder.feedback_pulse           = needs_feedback;
    builder.feedback_long_hold_level = false;
    return builder;
}

key_runtime_effect_builder_t key_runtime_slot_policy_take_flush(active_key_state_t *slot, bool active_held_action_survives_flush) {
    key_runtime_effect_builder_t builder = {0};

    if (!key_runtime_slot_active(slot)) {
        return builder;
    }

    if (!key_runtime_slot_allows_tap_release(slot) || slot->lifecycle.held_action_keycode != KC_NO || slot->lifecycle.repeat_binding_active) {
        if (slot->lifecycle.held_action_keycode != KC_NO && !active_held_action_survives_flush) {
            builder.kind   = KEY_RUNTIME_EFFECT_BUILDER_HELD_UNREGISTER;
            builder.action = slot->lifecycle.held_action_keycode;
        }
    } else if (!is_layer_key(slot->owner.keycode) && slot->binding.tap_action != KC_NO) {
        builder.kind   = KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION;
        builder.action = slot->binding.tap_action;
    }

    key_runtime_slot_reset(slot);
    return builder;
}

key_runtime_effect_builder_t key_runtime_slot_policy_fire_hold_at_threshold(active_key_state_t *slot, hold_behavior_t hold, hold_behavior_t long_hold, bool pulse_momentary_layer_action) {
    key_runtime_effect_builder_t builder = {0};

    if (!slot) {
        return builder;
    }

    switch (key_runtime_slot_policy_hold_threshold_dispatch_kind(hold)) {
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_slot_policy_clear_owned_hold(slot, &builder);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION;
            builder.action                   = hold.action;
            builder.feedback_pulse           = true;
            builder.feedback_long_hold_level = false;
            key_runtime_slot_commit_hold_phase(slot, !long_hold.present);
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_HELD:
            slot->lifecycle.held_action_keycode = hold.action;
            key_runtime_slot_commit_hold_phase(slot, !long_hold.present);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER;
            builder.action                   = hold.action;
            builder.feedback_pulse           = key_runtime_slot_policy_hold_activation_needs_pulse(hold, pulse_momentary_layer_action);
            builder.feedback_long_hold_level = false;
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_REPEAT:
            key_runtime_slot_policy_clear_owned_hold(slot, &builder);
            slot->lifecycle.repeat_binding_active = true;
            key_runtime_slot_commit_hold_phase(slot, !long_hold.present);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_REPEAT_START;
            builder.action                   = hold.action;
            builder.repeat_hz                = hold.repeat_hz;
            builder.feedback_pulse           = true;
            builder.feedback_long_hold_level = false;
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_NONE:
        default:
            return builder;
    }
}

key_runtime_effect_builder_t key_runtime_slot_policy_promote_to_long_hold(active_key_state_t *slot, hold_behavior_t long_hold, bool pulse_momentary_layer_action) {
    key_runtime_effect_builder_t builder = {0};

    if (!slot) {
        return builder;
    }

    key_runtime_slot_policy_clear_owned_hold(slot, &builder);

    switch (key_runtime_slot_policy_hold_threshold_dispatch_kind(long_hold)) {
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_slot_commit_hold_phase(slot, true);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION;
            builder.action                   = long_hold.action;
            builder.feedback_pulse           = true;
            builder.feedback_long_hold_level = true;
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_HELD:
            slot->lifecycle.held_action_keycode = long_hold.action;
            key_runtime_slot_commit_hold_phase(slot, true);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER;
            builder.action                   = long_hold.action;
            builder.feedback_pulse           = key_runtime_slot_policy_hold_activation_needs_pulse(long_hold, pulse_momentary_layer_action);
            builder.feedback_long_hold_level = true;
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_REPEAT:
            slot->lifecycle.repeat_binding_active = true;
            key_runtime_slot_commit_hold_phase(slot, true);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_REPEAT_START;
            builder.action                   = long_hold.action;
            builder.repeat_hz                = long_hold.repeat_hz;
            builder.feedback_pulse           = true;
            builder.feedback_long_hold_level = true;
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_NONE:
        default:
            return builder;
    }
}
