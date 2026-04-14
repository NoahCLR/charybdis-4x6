// ────────────────────────────────────────────────────────────────────────────
// Handled Key Policy
// ────────────────────────────────────────────────────────────────────────────
//
// Internal policy builders for handled-key hold/release contracts.
// Keep these out of the public handled-key API so authored lookup and
// contextual materialization remain the main external seam.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../action/action_kind_internal.h"
#include "handled_key.h"

static inline bool handled_key_hold_action_keeps_registered_feedback(noah_action_desc_t desc) {
    return noah_action_desc_keeps_registered_feedback(desc);
}

static inline uint8_t handled_key_hold_preview_layer(key_runtime_slot_hold_strategy_t hold_strategy, hold_behavior_t hold, noah_action_desc_t desc) {
    if (hold_strategy != KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT) {
        return UINT8_MAX;
    }

    if (hold.mode != HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE) {
        return UINT8_MAX;
    }

    return noah_action_desc_preview_layer(desc);
}

static inline handled_key_hold_semantics_t handled_key_hold_semantics_for_behavior(key_runtime_slot_hold_strategy_t hold_strategy, uint16_t flags, hold_behavior_t hold) {
    handled_key_hold_semantics_t semantics = {
        .preview_layer = UINT8_MAX,
    };

    if (!hold.present) {
        return semantics;
    }

    noah_action_desc_t desc = noah_action_describe(hold.action);

    semantics.release_layer_before_action = (flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0 && noah_action_desc_is_layer_lock(desc);
    semantics.preview_layer               = handled_key_hold_preview_layer(hold_strategy, hold, desc);

    switch (hold.mode) {
        case HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE:
            semantics.threshold_action          = hold.action;
            semantics.keeps_registered_feedback = handled_key_hold_action_keeps_registered_feedback(desc);
            return semantics;
        case HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE:
            semantics.threshold                 = HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD;
            semantics.threshold_action          = hold.action;
            semantics.uses_held_lifecycle       = !noah_action_desc_is_press_only(desc);
            semantics.keeps_registered_feedback = handled_key_hold_action_keeps_registered_feedback(desc);
            return semantics;
        case HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD:
            semantics.threshold        = HANDLED_KEY_HOLD_THRESHOLD_DISPATCH;
            semantics.threshold_action = hold.action;
            return semantics;
        case HOLD_BEHAVIOR_REPEAT_WHILE_HELD:
            semantics.threshold                 = HANDLED_KEY_HOLD_THRESHOLD_REPEAT;
            semantics.threshold_action          = hold.action;
            semantics.keeps_registered_feedback = true;
            return semantics;
        case HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD:
            semantics.release_action         = hold.action;
            semantics.keeps_pending_feedback = true;
            return semantics;
        case HOLD_BEHAVIOR_NONE:
        default:
            return semantics;
    }
}

static inline handled_key_hold_contract_t handled_key_hold_contract_for_behavior(key_runtime_slot_hold_strategy_t hold_strategy, uint16_t flags, hold_behavior_t hold) {
    return handled_key_hold_semantics_for_behavior(hold_strategy, flags, hold);
}

static inline handled_key_behavior_contract_t handled_key_behavior_contract(key_runtime_slot_hold_strategy_t hold_strategy, uint16_t flags, uint16_t tap_action, pd_mode_mask_t pd_mode, hold_behavior_t hold, hold_behavior_t long_hold) {
    return (handled_key_behavior_contract_t){
        .hold                                      = handled_key_hold_semantics_for_behavior(hold_strategy, flags, hold),
        .long_hold                                 = handled_key_hold_semantics_for_behavior(hold_strategy, flags, long_hold),
        .quick_tap_pd_mode_lock                    = pd_mode,
        .suppress_tap_on_layer_interrupt           = (flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0,
        .buffered_base_tap_dispatches_tap          = (flags & HANDLED_KEY_FLAG_FALLBACK_HOLD) != 0 && tap_action == KC_NO,
        .quick_release_of_immediate_hold_dispatches_tap = hold_registers_on_press(hold),
        .fallback_hold_suppresses_nonquick_release = (flags & HANDLED_KEY_FLAG_FALLBACK_HOLD) != 0,
        .nonquick_release_dispatches_tap           = (flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) == 0 && tap_action != KC_NO,
    };
}
