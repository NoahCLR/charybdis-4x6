// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Slot Policy
// ────────────────────────────────────────────────────────────────────────────

#include "key_runtime_slot_policy.h"

#include "../../../action/action_dispatch.h"
#include "../../../action/action_lifecycle.h"
#include "../key_runtime_index.h"
#include "../key_runtime_trace.h"

typedef enum {
    KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_NONE = 0,
    KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_TAP,
    KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_HELD,
    KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_REPEAT,
} key_runtime_slot_policy_hold_threshold_dispatch_t;

static key_runtime_slot_policy_hold_threshold_dispatch_t key_runtime_slot_policy_hold_threshold_dispatch_kind(handled_key_hold_contract_t contract) {
    switch (contract.threshold) {
        case HANDLED_KEY_HOLD_THRESHOLD_DISPATCH:
            return KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_TAP;
        case HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD:
            return KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_HELD;
        case HANDLED_KEY_HOLD_THRESHOLD_REPEAT:
            return KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_REPEAT;
        case HANDLED_KEY_HOLD_THRESHOLD_NONE:
        default:
            return KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_NONE;
    }
}

static bool key_runtime_slot_policy_hold_activation_needs_pulse(hold_behavior_t hold, handled_key_hold_contract_t contract, bool pulse_momentary_layer_action) {
    noah_action_desc_t desc = noah_action_describe(hold.action);

    if (contract.threshold == HANDLED_KEY_HOLD_THRESHOLD_DISPATCH) {
        return true;
    }

    if (noah_action_desc_is_owned_momentary_layer(desc)) {
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
        key_runtime_index_rebuild();
    }
}

static key_runtime_trace_hold_dispatch_t key_runtime_slot_policy_trace_dispatch(key_runtime_effect_builder_t builder) {
    switch (builder.kind) {
        case KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION:
            return KEY_RUNTIME_TRACE_HOLD_DISPATCH_ACTION;
        case KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER:
        case KEY_RUNTIME_EFFECT_BUILDER_HELD_UNREGISTER:
            return KEY_RUNTIME_TRACE_HOLD_DISPATCH_HELD;
        case KEY_RUNTIME_EFFECT_BUILDER_REPEAT_START:
            return KEY_RUNTIME_TRACE_HOLD_DISPATCH_REPEAT;
        case KEY_RUNTIME_EFFECT_BUILDER_NONE:
        default:
            return KEY_RUNTIME_TRACE_HOLD_DISPATCH_NONE;
    }
}

static uint16_t key_runtime_slot_policy_trace_flags(key_runtime_effect_builder_t builder, bool completes_hold) {
    uint16_t flags = 0;

    if (completes_hold) {
        flags |= KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_COMPLETES_HOLD;
    }
    if (builder.feedback_pulse) {
        flags |= KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_FEEDBACK_PULSE;
    }
    if (builder.feedback_long_hold_level) {
        flags |= KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_FEEDBACK_LONG;
    }
    if (builder.release_owned_state) {
        flags |= KEY_RUNTIME_TRACE_HOLD_POLICY_FLAG_RELEASE_OWNED_KEY;
    }

    return flags;
}

static void key_runtime_slot_policy_trace(key_runtime_trace_hold_policy_decision_t decision, key_runtime_effect_builder_t builder, bool completes_hold) {
    key_runtime_trace_hold_policy_decision(decision, key_runtime_slot_policy_trace_dispatch(builder), key_runtime_slot_policy_trace_flags(builder, completes_hold), builder.action);
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
    key_runtime_slot_policy_trace(KEY_RUNTIME_TRACE_HOLD_POLICY_ACTIVATE_PENDING_FALLBACK, builder, true);
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
    key_runtime_slot_policy_trace(KEY_RUNTIME_TRACE_HOLD_POLICY_COMMIT_IMMEDIATE_HOLD, builder, completes_hold);
    return builder;
}

key_runtime_effect_builder_t key_runtime_slot_policy_take_flush(active_key_state_t *slot, bool active_held_action_survives_flush) {
    key_runtime_effect_builder_t   builder = {0};
    key_runtime_slot_interaction_t interaction;

    if (!key_runtime_slot_active(slot)) {
        return builder;
    }

    interaction = key_runtime_slot_cached_interaction(slot);

    if (!key_runtime_slot_allows_tap_release(slot) || slot->lifecycle.held_action_keycode != KC_NO || slot->lifecycle.repeat_binding_active) {
        if (slot->lifecycle.held_action_keycode != KC_NO && !active_held_action_survives_flush) {
            builder.kind   = KEY_RUNTIME_EFFECT_BUILDER_HELD_UNREGISTER;
            builder.action = slot->lifecycle.held_action_keycode;
        }
    } else if (!is_layer_key(slot->owner.keycode) && interaction.binding.tap_action != KC_NO) {
        builder.kind   = KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION;
        builder.action = interaction.binding.tap_action;
    }

    key_runtime_slot_reset(slot);
    return builder;
}

key_runtime_effect_builder_t key_runtime_slot_policy_fire_hold_at_threshold(active_key_state_t *slot, hold_behavior_t hold, handled_key_hold_contract_t contract, bool completes_hold, bool pulse_momentary_layer_action) {
    key_runtime_effect_builder_t builder = {0};

    if (!slot) {
        return builder;
    }

    switch (key_runtime_slot_policy_hold_threshold_dispatch_kind(contract)) {
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_slot_policy_clear_owned_hold(slot, &builder);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION;
            builder.action                   = hold.action;
            builder.feedback_pulse           = true;
            builder.feedback_long_hold_level = false;
            key_runtime_slot_commit_hold_phase(slot, completes_hold);
            key_runtime_slot_policy_trace(KEY_RUNTIME_TRACE_HOLD_POLICY_FIRE_THRESHOLD, builder, completes_hold);
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_HELD:
            slot->lifecycle.held_action_keycode = hold.action;
            key_runtime_slot_commit_hold_phase(slot, completes_hold);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER;
            builder.action                   = hold.action;
            builder.feedback_pulse           = key_runtime_slot_policy_hold_activation_needs_pulse(hold, contract, pulse_momentary_layer_action);
            builder.feedback_long_hold_level = false;
            key_runtime_slot_policy_trace(KEY_RUNTIME_TRACE_HOLD_POLICY_FIRE_THRESHOLD, builder, completes_hold);
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_REPEAT:
            key_runtime_slot_policy_clear_owned_hold(slot, &builder);
            slot->lifecycle.repeat_binding_active = true;
            key_runtime_slot_commit_hold_phase(slot, completes_hold);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_REPEAT_START;
            builder.action                   = hold.action;
            builder.repeat_hz                = hold.repeat_hz;
            builder.feedback_pulse           = true;
            builder.feedback_long_hold_level = false;
            key_runtime_slot_policy_trace(KEY_RUNTIME_TRACE_HOLD_POLICY_FIRE_THRESHOLD, builder, completes_hold);
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_NONE:
        default:
            key_runtime_slot_policy_trace(KEY_RUNTIME_TRACE_HOLD_POLICY_FIRE_THRESHOLD, builder, completes_hold);
            return builder;
    }
}

key_runtime_effect_builder_t key_runtime_slot_policy_promote_to_long_hold(active_key_state_t *slot, hold_behavior_t long_hold, handled_key_hold_contract_t contract, bool pulse_momentary_layer_action) {
    key_runtime_effect_builder_t builder = {0};

    if (!slot) {
        return builder;
    }

    key_runtime_slot_policy_clear_owned_hold(slot, &builder);

    switch (key_runtime_slot_policy_hold_threshold_dispatch_kind(contract)) {
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_TAP:
            key_runtime_slot_commit_hold_phase(slot, true);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_DISPATCH_ACTION;
            builder.action                   = long_hold.action;
            builder.feedback_pulse           = true;
            builder.feedback_long_hold_level = true;
            key_runtime_slot_policy_trace(KEY_RUNTIME_TRACE_HOLD_POLICY_PROMOTE_LONG_HOLD, builder, true);
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_HELD:
            slot->lifecycle.held_action_keycode = long_hold.action;
            key_runtime_slot_commit_hold_phase(slot, true);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_HELD_REGISTER;
            builder.action                   = long_hold.action;
            builder.feedback_pulse           = key_runtime_slot_policy_hold_activation_needs_pulse(long_hold, contract, pulse_momentary_layer_action);
            builder.feedback_long_hold_level = true;
            key_runtime_slot_policy_trace(KEY_RUNTIME_TRACE_HOLD_POLICY_PROMOTE_LONG_HOLD, builder, true);
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_REPEAT:
            slot->lifecycle.repeat_binding_active = true;
            key_runtime_slot_commit_hold_phase(slot, true);
            builder.kind                     = KEY_RUNTIME_EFFECT_BUILDER_REPEAT_START;
            builder.action                   = long_hold.action;
            builder.repeat_hz                = long_hold.repeat_hz;
            builder.feedback_pulse           = true;
            builder.feedback_long_hold_level = true;
            key_runtime_slot_policy_trace(KEY_RUNTIME_TRACE_HOLD_POLICY_PROMOTE_LONG_HOLD, builder, true);
            return builder;
        case KEY_RUNTIME_SLOT_POLICY_HOLD_THRESHOLD_DISPATCH_NONE:
        default:
            key_runtime_slot_policy_trace(KEY_RUNTIME_TRACE_HOLD_POLICY_PROMOTE_LONG_HOLD, builder, true);
            return builder;
    }
}
