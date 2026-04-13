// ────────────────────────────────────────────────────────────────────────────
// Handled Key View
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime helpers that adapt authored key_behavior rows into a fully resolved
// handled-key contract for the press/release engine.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../action/action_dispatch.h"
#include "../../pointing/defs/pd_mode_flags.h"
#include "../runtime/key_runtime_types.h"
#include "key_behavior.h"

typedef struct {
    uint16_t                         tap_action;
    uint8_t                          tap_repeat_count;
    hold_behavior_t                  hold;
    hold_behavior_t                  long_hold;
    key_runtime_slot_hold_strategy_t hold_strategy;
    uint16_t                         tap_hold_term;
    uint16_t                         longer_hold_term;
    uint16_t                         multi_tap_term;
    uint8_t                          layer;
    pd_mode_mask_t                   pd_mode;
    bool                             step_present;
    bool                             has_more_taps;
    bool                             tap_resolves_on_press;
    uint16_t                         flags;
} handled_key_view_t;

typedef enum {
    HANDLED_KEY_FLAG_HANDLED         = (1u << 0),
    HANDLED_KEY_FLAG_MULTI_TAP       = (1u << 1),
    HANDLED_KEY_FLAG_IMPLICIT_HOLD   = (1u << 2),
    HANDLED_KEY_FLAG_FALLBACK_HOLD   = (1u << 3),
    HANDLED_KEY_FLAG_MOMENTARY_LAYER = (1u << 4),
    HANDLED_KEY_FLAG_LAYER_TAP       = (1u << 5),
} handled_key_flag_t;

typedef enum {
    HANDLED_KEY_HOLD_THRESHOLD_NONE = 0,
    HANDLED_KEY_HOLD_THRESHOLD_DISPATCH,
    HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD,
    HANDLED_KEY_HOLD_THRESHOLD_REPEAT,
} handled_key_hold_threshold_t;

typedef struct {
    handled_key_hold_threshold_t threshold;
    bool                         dispatches_on_release;
    bool                         release_uses_held_lifecycle;
    bool                         keeps_registered_feedback;
    bool                         keeps_pending_feedback;
    bool                         release_layer_before_action;
    uint8_t                      preview_layer;
} handled_key_hold_contract_t;

typedef struct {
    handled_key_hold_contract_t hold;
    handled_key_hold_contract_t long_hold;
} handled_key_interaction_policy_t;

static inline bool handled_key_hold_contract_fires_at_threshold(handled_key_hold_contract_t contract) {
    return contract.threshold != HANDLED_KEY_HOLD_THRESHOLD_NONE;
}

static inline bool handled_key_hold_contract_registers_held(handled_key_hold_contract_t contract) {
    return contract.threshold == HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD;
}

static inline bool handled_key_hold_contract_repeats_while_held(handled_key_hold_contract_t contract) {
    return contract.threshold == HANDLED_KEY_HOLD_THRESHOLD_REPEAT;
}

static inline bool handled_key_hold_action_keeps_registered_feedback(noah_action_desc_t desc) {
    return !(noah_action_desc_is_layer_action(desc) || noah_action_desc_is_pd_mode_action(desc));
}

static inline uint8_t handled_key_hold_preview_layer(handled_key_view_t key, hold_behavior_t hold, noah_action_desc_t desc) {
    if (key.hold_strategy != KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT) {
        return UINT8_MAX;
    }

    if (hold.mode != HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE || !noah_action_desc_is_owned_momentary_layer(desc)) {
        return UINT8_MAX;
    }

    return desc.layer;
}

static inline handled_key_hold_contract_t handled_key_hold_contract_for_behavior(handled_key_view_t key, hold_behavior_t hold) {
    handled_key_hold_contract_t contract = {
        .preview_layer = UINT8_MAX,
    };

    if (!hold.present) {
        return contract;
    }

    noah_action_desc_t desc = noah_action_describe(hold.action);

    contract.release_layer_before_action = (key.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0 && noah_action_desc_is_layer_lock(desc);
    contract.preview_layer               = handled_key_hold_preview_layer(key, hold, desc);

    switch (hold.mode) {
        case HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE:
            contract.keeps_registered_feedback = handled_key_hold_action_keeps_registered_feedback(desc);
            return contract;
        case HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE:
            contract.threshold                   = HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD;
            contract.release_uses_held_lifecycle = !noah_action_desc_is_press_only(desc);
            contract.keeps_registered_feedback   = handled_key_hold_action_keeps_registered_feedback(desc);
            return contract;
        case HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD:
            contract.threshold = HANDLED_KEY_HOLD_THRESHOLD_DISPATCH;
            return contract;
        case HOLD_BEHAVIOR_REPEAT_WHILE_HELD:
            contract.threshold                 = HANDLED_KEY_HOLD_THRESHOLD_REPEAT;
            contract.keeps_registered_feedback = true;
            return contract;
        case HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD:
            contract.dispatches_on_release = true;
            contract.keeps_pending_feedback = true;
            return contract;
        case HOLD_BEHAVIOR_NONE:
        default:
            return contract;
    }
}

static inline handled_key_interaction_policy_t handled_key_resolve_policy(handled_key_view_t key) {
    return (handled_key_interaction_policy_t){
        .hold      = handled_key_hold_contract_for_behavior(key, key.hold),
        .long_hold = handled_key_hold_contract_for_behavior(key, key.long_hold),
    };
}

handled_key_view_t               handled_key_lookup(uint16_t keycode);
handled_key_view_t               handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count);
bool                             handled_key_is_handled(handled_key_view_t key);
bool                             handled_key_uses_implicit_hold(handled_key_view_t key);
bool                             handled_key_uses_fallback_hold(handled_key_view_t key);
bool                             handled_key_has_multi_tap(handled_key_view_t key);
bool                             handled_key_is_momentary_layer(handled_key_view_t key);
bool                             handled_key_is_layer_tap(handled_key_view_t key);
hold_behavior_t                  handled_key_single_hold(handled_key_view_t key);
hold_behavior_t                  handled_key_long_hold(handled_key_view_t key);
key_runtime_slot_hold_strategy_t handled_key_hold_strategy(handled_key_view_t key);
uint16_t                         handled_key_tap_action(handled_key_view_t key);
uint8_t                          handled_key_tap_repeat_count(handled_key_view_t key);
uint16_t                         handled_key_tap_hold_term(handled_key_view_t key);
uint16_t                         handled_key_longer_hold_term(handled_key_view_t key);
uint16_t                         handled_key_multi_tap_term(handled_key_view_t key);
uint8_t                          handled_key_layer(handled_key_view_t key);
pd_mode_mask_t                   handled_key_pd_mode(handled_key_view_t key);
