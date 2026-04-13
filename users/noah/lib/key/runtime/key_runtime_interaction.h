// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Interaction
// ────────────────────────────────────────────────────────────────────────────
//
// Slot-owned interaction contract cached by the key runtime after a handled
// key press resolves. This is distinct from handled_key_resolution_t, which
// remains the authored lookup result from interaction/handled_key.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../interaction/handled_key.h"

typedef struct {
    uint16_t     tap_action;
    uint16_t     release_hold_action;
    uint16_t     release_long_hold_action;
    pd_mode_mask_t quick_tap_pd_mode_lock;
    bool         suppress_tap_on_layer_interrupt;
    bool         buffered_base_tap_dispatches_tap;
    bool         quick_release_of_immediate_hold_dispatches_tap;
    bool         fallback_hold_suppresses_nonquick_release;
    bool         nonquick_release_dispatches_tap;
    bool         buffers_multi_tap;
} key_runtime_slot_release_contract_t;

typedef struct {
    union {
        handled_key_resolution_t resolution;
        struct {
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
        };
    };
    handled_key_interaction_policy_t policy;
} key_runtime_slot_interaction_t;

static inline key_runtime_slot_interaction_t key_runtime_slot_interaction_default(void) {
    return (key_runtime_slot_interaction_t){
        .resolution =
            {
                .tap_hold_term    = CUSTOM_TAP_HOLD_TERM,
                .longer_hold_term = CUSTOM_LONGER_HOLD_TERM,
                .multi_tap_term   = CUSTOM_MULTI_TAP_TERM,
                .layer            = UINT8_MAX,
            },
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
    };
}

static inline key_runtime_slot_interaction_t key_runtime_slot_interaction_from_resolution(handled_key_resolution_t resolution) {
    return (key_runtime_slot_interaction_t){
        .resolution = resolution,
        .policy     = handled_key_resolve_policy(resolution),
    };
}

static inline key_runtime_slot_interaction_t key_runtime_slot_interaction_from_handled_key(handled_key_view_t key) {
    return key_runtime_slot_interaction_from_resolution(key);
}

static inline handled_key_view_t key_runtime_slot_interaction_to_handled_key_view(key_runtime_slot_interaction_t interaction) {
    return interaction.resolution;
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

static inline key_runtime_slot_release_contract_t key_runtime_slot_release_contract(key_runtime_slot_interaction_t interaction) {
    return (key_runtime_slot_release_contract_t){
        .tap_action                                   = interaction.tap_action,
        .release_hold_action                          = interaction.policy.hold.dispatches_on_release ? interaction.hold.action : KC_NO,
        .release_long_hold_action                     = interaction.policy.long_hold.dispatches_on_release ? interaction.long_hold.action : KC_NO,
        .quick_tap_pd_mode_lock                       = interaction.pd_mode,
        .suppress_tap_on_layer_interrupt              = key_runtime_slot_interaction_is_momentary_layer(interaction),
        .buffered_base_tap_dispatches_tap             = key_runtime_slot_interaction_uses_fallback_hold(interaction) && interaction.tap_action == KC_NO,
        .quick_release_of_immediate_hold_dispatches_tap = hold_registers_on_press(interaction.hold),
        .fallback_hold_suppresses_nonquick_release    = key_runtime_slot_interaction_uses_fallback_hold(interaction),
        .nonquick_release_dispatches_tap              = !key_runtime_slot_interaction_is_momentary_layer(interaction) && interaction.tap_action != KC_NO,
        .buffers_multi_tap                            = (interaction.flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0,
    };
}

static inline uint16_t key_runtime_slot_release_contract_select_hold_action(key_runtime_slot_release_contract_t contract, uint16_t elapsed, uint16_t longer_hold_term) {
    if (contract.release_long_hold_action != KC_NO && elapsed >= longer_hold_term) {
        return contract.release_long_hold_action;
    }

    return contract.release_hold_action;
}
