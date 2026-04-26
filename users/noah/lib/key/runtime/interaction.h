// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Interaction
// ────────────────────────────────────────────────────────────────────────────
//
// Slot-owned interaction contract cached by the key runtime after a handled
// key press resolves. This is distinct from handled_key_resolution_t, which
// remains the authored lookup result from the handled-key interaction seam.
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
    uint16_t primary_action;
    uint16_t long_action;
} key_runtime_slot_release_hold_contract_t;

typedef struct {
    key_runtime_slot_release_tap_contract_t  tap;
    key_runtime_slot_release_hold_contract_t hold;
    pd_mode_mask_t                           quick_tap_pd_mode_lock;
    bool                                     suppress_tap_on_layer_interrupt;
    bool                                     buffered_base_tap_dispatches_tap;
    bool                                     quick_release_of_immediate_hold_dispatches_tap;
    bool                                     fallback_hold_suppresses_nonquick_release;
    bool                                     nonquick_release_dispatches_tap;
} key_runtime_slot_release_contract_t;

typedef struct {
    uint16_t            keycode;
    uint8_t             tap_count;
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
    uint16_t        branch_confirm_term;
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
    handled_key_behavior_contract_t  contract;
} key_runtime_slot_interaction_t;

static inline key_runtime_slot_selection_t key_runtime_slot_selection_from_resolution(handled_key_resolution_t resolution) {
    return (key_runtime_slot_selection_t){
        .keycode   = resolution.keycode,
        .tap_count = resolution.tap_count,
        .step      = resolution.step,
    };
}

static inline key_runtime_slot_binding_t key_runtime_slot_binding_from_materialized(handled_key_materialized_t materialized) {
    return (key_runtime_slot_binding_t){
        .tap_action            = materialized.tap_action,
        .tap_repeat_count      = materialized.tap_repeat_count,
        .hold                  = materialized.hold,
        .long_hold             = materialized.long_hold,
        .tap_hold_term         = materialized.authored.tap_hold_term,
        .longer_hold_term      = materialized.authored.longer_hold_term,
        .multi_tap_term        = materialized.authored.multi_tap_term,
        .branch_confirm_term   = materialized.authored.branch_confirm_term,
        .has_more_taps         = materialized.tap_has_more_taps,
        .tap_resolves_on_press = materialized.tap_resolves_on_press,
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

static inline key_runtime_slot_release_hold_contract_t key_runtime_slot_release_hold_contract_build(key_runtime_slot_interaction_t interaction) {
    return (key_runtime_slot_release_hold_contract_t){
        .primary_action = interaction.contract.hold.release_action,
        .long_action    = interaction.contract.long_hold.release_action,
    };
}

static inline key_runtime_slot_release_contract_t key_runtime_slot_release_contract_build(key_runtime_slot_interaction_t interaction) {
    return (key_runtime_slot_release_contract_t){
        .tap                                            = key_runtime_slot_release_tap_contract_build(interaction),
        .hold                                           = key_runtime_slot_release_hold_contract_build(interaction),
        .quick_tap_pd_mode_lock                         = interaction.contract.quick_tap_pd_mode_lock,
        .suppress_tap_on_layer_interrupt                = interaction.contract.suppress_tap_on_layer_interrupt,
        .buffered_base_tap_dispatches_tap               = interaction.contract.buffered_base_tap_dispatches_tap,
        .quick_release_of_immediate_hold_dispatches_tap = interaction.contract.quick_release_of_immediate_hold_dispatches_tap,
        .fallback_hold_suppresses_nonquick_release      = interaction.contract.fallback_hold_suppresses_nonquick_release,
        .nonquick_release_dispatches_tap                = interaction.contract.nonquick_release_dispatches_tap,
    };
}

static inline key_runtime_slot_interaction_t key_runtime_slot_interaction_default(void) {
    return (key_runtime_slot_interaction_t){
        .binding =
            {
                .tap_hold_term    = CUSTOM_TAP_HOLD_TERM,
                .longer_hold_term = CUSTOM_LONGER_HOLD_TERM,
                .multi_tap_term   = CUSTOM_MULTI_TAP_TERM,
                .branch_confirm_term = CUSTOM_TAP_BRANCH_CONFIRM_TERM,
            },
        .layer = UINT8_MAX,
        .contract =
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

static inline key_runtime_slot_interaction_t key_runtime_slot_interaction_from_materialized(handled_key_materialized_t materialized) {
    return (key_runtime_slot_interaction_t){
        .selection     = key_runtime_slot_selection_from_resolution(materialized.authored),
        .binding       = key_runtime_slot_binding_from_materialized(materialized),
        .hold_strategy = materialized.hold_strategy,
        .layer         = materialized.layer,
        .pd_mode       = materialized.pd_mode,
        .flags         = materialized.flags,
        .contract      = materialized.contract,
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
    return key_runtime_slot_release_contract_build(interaction);
}

static inline bool key_runtime_slot_release_hold_contract_has_primary_action(key_runtime_slot_release_hold_contract_t contract) {
    return contract.primary_action != KC_NO;
}

static inline bool key_runtime_slot_release_hold_contract_has_any_action(key_runtime_slot_release_hold_contract_t contract) {
    return contract.primary_action != KC_NO || contract.long_action != KC_NO;
}

static inline bool key_runtime_slot_release_hold_contract_long_ready(key_runtime_slot_release_hold_contract_t contract, uint16_t elapsed, uint16_t longer_hold_term) {
    return contract.long_action != KC_NO && elapsed >= longer_hold_term;
}

static inline uint16_t key_runtime_slot_release_hold_contract_select_action(key_runtime_slot_release_hold_contract_t contract, uint16_t elapsed, uint16_t longer_hold_term) {
    if (key_runtime_slot_release_hold_contract_long_ready(contract, elapsed, longer_hold_term)) {
        return contract.long_action;
    }

    return contract.primary_action;
}

static inline uint16_t key_runtime_slot_release_contract_select_hold_action(key_runtime_slot_release_contract_t contract, uint16_t elapsed, uint16_t longer_hold_term) {
    return key_runtime_slot_release_hold_contract_select_action(contract.hold, elapsed, longer_hold_term);
}
