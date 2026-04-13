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

typedef enum {
    KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_NONE = 0,
    KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_DISPATCH_ACTION,
    KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_BUFFER_MULTI_TAP,
} key_runtime_slot_release_tap_outcome_t;

typedef struct {
    key_runtime_slot_release_tap_outcome_t outcome;
    uint16_t                               action;
    uint8_t                                repeat_count;
} key_runtime_slot_release_tap_contract_t;

typedef struct {
    key_runtime_slot_release_tap_contract_t tap;
    uint16_t                                release_hold_action;
    uint16_t                                release_long_hold_action;
    pd_mode_mask_t                          quick_tap_pd_mode_lock;
    bool                                    suppress_tap_on_layer_interrupt;
    bool                                    buffered_base_tap_dispatches_tap;
    bool                                    quick_release_of_immediate_hold_dispatches_tap;
    bool                                    fallback_hold_suppresses_nonquick_release;
    bool                                    nonquick_release_dispatches_tap;
} key_runtime_slot_release_contract_t;

typedef struct {
    uint16_t        keycode;
    uint8_t         tap_count;
    key_behavior_step_t step;
} key_runtime_slot_selection_t;

typedef struct {
    uint16_t        tap_action;
    uint8_t         tap_repeat_count;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
    uint16_t        tap_hold_term;
    uint16_t        longer_hold_term;
    uint16_t        multi_tap_term;
    bool            has_more_taps;
    bool            tap_resolves_on_press;
} key_runtime_slot_binding_t;

typedef struct {
    key_runtime_slot_selection_t     selection;
    key_runtime_slot_binding_t       binding;
    key_runtime_slot_hold_strategy_t hold_strategy;
    uint8_t                          layer;
    pd_mode_mask_t                   pd_mode;
    uint16_t                         flags;
    handled_key_interaction_policy_t policy;
    key_runtime_slot_release_contract_t release;
} key_runtime_slot_interaction_t;

static inline key_runtime_slot_selection_t key_runtime_slot_selection_from_resolution(handled_key_resolution_t resolution) {
    return (key_runtime_slot_selection_t){
        .keycode   = resolution.keycode,
        .tap_count = resolution.tap_count,
        .step      = resolution.step,
    };
}

static inline key_runtime_slot_binding_t key_runtime_slot_binding_from_resolution(handled_key_resolution_t resolution) {
    return (key_runtime_slot_binding_t){
        .tap_action            = handled_key_resolution_tap_action(resolution),
        .tap_repeat_count      = handled_key_resolution_tap_repeat_count(resolution),
        .hold                  = handled_key_resolution_hold(resolution),
        .long_hold             = handled_key_resolution_long_hold(resolution),
        .tap_hold_term         = resolution.tap_hold_term,
        .longer_hold_term      = resolution.longer_hold_term,
        .multi_tap_term        = resolution.multi_tap_term,
        .has_more_taps         = resolution.has_more_taps,
        .tap_resolves_on_press = handled_key_resolution_tap_resolves_on_press(resolution),
    };
}

static inline key_runtime_slot_release_tap_contract_t key_runtime_slot_release_tap_contract_build(key_runtime_slot_interaction_t interaction) {
    if ((interaction.flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0) {
        return (key_runtime_slot_release_tap_contract_t){
            .outcome      = KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_BUFFER_MULTI_TAP,
            .action       = interaction.binding.tap_action,
            .repeat_count = interaction.binding.tap_repeat_count,
        };
    }

    if (interaction.binding.tap_action != KC_NO) {
        return (key_runtime_slot_release_tap_contract_t){
            .outcome      = KEY_RUNTIME_SLOT_RELEASE_TAP_OUTCOME_DISPATCH_ACTION,
            .action       = interaction.binding.tap_action,
            .repeat_count = interaction.binding.tap_repeat_count,
        };
    }

    return (key_runtime_slot_release_tap_contract_t){0};
}

static inline key_runtime_slot_release_contract_t key_runtime_slot_release_contract_build(key_runtime_slot_interaction_t interaction) {
    return (key_runtime_slot_release_contract_t){
        .tap                                          = key_runtime_slot_release_tap_contract_build(interaction),
        .release_hold_action                          = interaction.policy.hold.dispatches_on_release ? interaction.binding.hold.action : KC_NO,
        .release_long_hold_action                     = interaction.policy.long_hold.dispatches_on_release ? interaction.binding.long_hold.action : KC_NO,
        .quick_tap_pd_mode_lock                       = interaction.pd_mode,
        .suppress_tap_on_layer_interrupt              = (interaction.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0,
        .buffered_base_tap_dispatches_tap             = (interaction.flags & HANDLED_KEY_FLAG_FALLBACK_HOLD) != 0 && interaction.binding.tap_action == KC_NO,
        .quick_release_of_immediate_hold_dispatches_tap = hold_registers_on_press(interaction.binding.hold),
        .fallback_hold_suppresses_nonquick_release    = (interaction.flags & HANDLED_KEY_FLAG_FALLBACK_HOLD) != 0,
        .nonquick_release_dispatches_tap              = (interaction.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) == 0 && interaction.binding.tap_action != KC_NO,
    };
}

static inline key_runtime_slot_interaction_t key_runtime_slot_interaction_default(void) {
    return (key_runtime_slot_interaction_t){
        .binding =
            {
                .tap_hold_term    = CUSTOM_TAP_HOLD_TERM,
                .longer_hold_term = CUSTOM_LONGER_HOLD_TERM,
                .multi_tap_term   = CUSTOM_MULTI_TAP_TERM,
            },
        .layer  = UINT8_MAX,
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
    key_runtime_slot_interaction_t interaction = {
        .selection     = key_runtime_slot_selection_from_resolution(resolution),
        .binding       = key_runtime_slot_binding_from_resolution(resolution),
        .hold_strategy = handled_key_resolution_hold_strategy(resolution),
        .layer         = resolution.layer,
        .pd_mode       = resolution.pd_mode,
        .flags         = resolution.flags,
    };

    if (handled_key_resolution_uses_implicit_hold(resolution)) {
        interaction.flags |= HANDLED_KEY_FLAG_IMPLICIT_HOLD;
    }
    if (handled_key_resolution_uses_fallback_hold(resolution)) {
        interaction.flags |= HANDLED_KEY_FLAG_FALLBACK_HOLD;
    }

    interaction.policy  = handled_key_interaction_policy(interaction.hold_strategy, interaction.flags, interaction.binding.hold, interaction.binding.long_hold);
    interaction.release = key_runtime_slot_release_contract_build(interaction);
    return interaction;
}

static inline handled_key_resolution_t key_runtime_slot_interaction_to_resolution(key_runtime_slot_interaction_t interaction) {
    return (handled_key_resolution_t){
        .keycode               = interaction.selection.keycode,
        .tap_count             = interaction.selection.tap_count,
        .step                  = interaction.selection.step,
        .tap_hold_term         = interaction.binding.tap_hold_term,
        .longer_hold_term      = interaction.binding.longer_hold_term,
        .multi_tap_term        = interaction.binding.multi_tap_term,
        .layer                 = interaction.layer,
        .pd_mode               = interaction.pd_mode,
        .has_more_taps         = interaction.binding.has_more_taps,
        .flags                 = interaction.flags & (uint16_t)~(HANDLED_KEY_FLAG_IMPLICIT_HOLD | HANDLED_KEY_FLAG_FALLBACK_HOLD),
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

static inline key_runtime_slot_release_contract_t key_runtime_slot_release_contract(key_runtime_slot_interaction_t interaction) {
    return interaction.release;
}

static inline uint16_t key_runtime_slot_release_contract_select_hold_action(key_runtime_slot_release_contract_t contract, uint16_t elapsed, uint16_t longer_hold_term) {
    if (contract.release_long_hold_action != KC_NO && elapsed >= longer_hold_term) {
        return contract.release_long_hold_action;
    }

    return contract.release_hold_action;
}
