// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Interaction
// ────────────────────────────────────────────────────────────────────────────
//
// Slot-owned interaction contract cached by the key runtime after a handled
// key press resolves. This is distinct from handled_key_view_t, which remains
// the authored lookup result from interaction/handled_key.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../interaction/handled_key.h"

typedef struct {
    uint16_t                         tap_action;
    uint8_t                          tap_repeat_count;
    hold_behavior_t                  hold;
    hold_behavior_t                  long_hold;
    handled_key_interaction_policy_t policy;
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
} key_runtime_slot_interaction_t;

static inline key_runtime_slot_interaction_t key_runtime_slot_interaction_default(void) {
    return (key_runtime_slot_interaction_t){
        .policy =
            {
                .hold =
                    {
                        .preview_layer = UINT8_MAX,
                    },
                .long_hold =
                    {
                        .preview_layer = UINT8_MAX,
                    },
            },
        .tap_hold_term    = CUSTOM_TAP_HOLD_TERM,
        .longer_hold_term = CUSTOM_LONGER_HOLD_TERM,
        .multi_tap_term   = CUSTOM_MULTI_TAP_TERM,
        .layer            = UINT8_MAX,
    };
}

static inline key_runtime_slot_interaction_t key_runtime_slot_interaction_from_handled_key(handled_key_view_t key) {
    return (key_runtime_slot_interaction_t){
        .tap_action       = key.tap_action,
        .tap_repeat_count = key.tap_repeat_count,
        .hold             = key.hold,
        .long_hold        = key.long_hold,
        .policy           = handled_key_resolve_policy(key),
        .hold_strategy    = key.hold_strategy,
        .tap_hold_term    = key.tap_hold_term,
        .longer_hold_term = key.longer_hold_term,
        .multi_tap_term   = key.multi_tap_term,
        .layer            = key.layer,
        .pd_mode          = key.pd_mode,
        .step_present     = key.step_present,
        .has_more_taps    = key.has_more_taps,
        .tap_resolves_on_press = key.tap_resolves_on_press,
        .flags            = key.flags,
    };
}

static inline handled_key_view_t key_runtime_slot_interaction_to_handled_key_view(key_runtime_slot_interaction_t interaction) {
    return (handled_key_view_t){
        .tap_action       = interaction.tap_action,
        .tap_repeat_count = interaction.tap_repeat_count,
        .hold             = interaction.hold,
        .long_hold        = interaction.long_hold,
        .hold_strategy    = interaction.hold_strategy,
        .tap_hold_term    = interaction.tap_hold_term,
        .longer_hold_term = interaction.longer_hold_term,
        .multi_tap_term   = interaction.multi_tap_term,
        .layer            = interaction.layer,
        .pd_mode          = interaction.pd_mode,
        .step_present     = interaction.step_present,
        .has_more_taps    = interaction.has_more_taps,
        .tap_resolves_on_press = interaction.tap_resolves_on_press,
        .flags            = interaction.flags,
    };
}

static inline bool key_runtime_slot_interaction_uses_implicit_hold(key_runtime_slot_interaction_t interaction) {
    return (interaction.flags & HANDLED_KEY_FLAG_IMPLICIT_HOLD) != 0;
}

static inline bool key_runtime_slot_interaction_uses_fallback_hold(key_runtime_slot_interaction_t interaction) {
    return (interaction.flags & HANDLED_KEY_FLAG_FALLBACK_HOLD) != 0;
}

static inline bool key_runtime_slot_interaction_is_momentary_layer(key_runtime_slot_interaction_t interaction) {
    return (interaction.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0;
}
