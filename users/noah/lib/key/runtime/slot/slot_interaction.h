// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Interaction
// ────────────────────────────────────────────────────────────────────────────
//
// Slot-owned interaction contract cached by the key runtime after a handled
// key press resolves. This is distinct from handled_key_resolution_t, which
// remains the authored lookup result from the handled-key behavior seam.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../behavior/handled_key.h"

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
    bool            authored_has_more_taps;
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

static inline key_runtime_slot_interaction_t key_runtime_slot_interaction_default(void) {
    return (key_runtime_slot_interaction_t){
        .binding =
            {
                .tap_hold_term       = CUSTOM_TAP_HOLD_TERM,
                .longer_hold_term    = CUSTOM_LONGER_HOLD_TERM,
                .multi_tap_term      = CUSTOM_MULTI_TAP_TERM,
                .branch_confirm_term = CUSTOM_RGB_BRANCH_CONFIRM_TERM,
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

static inline void key_runtime_slot_interaction_from_materialized_into(const handled_key_materialized_t *materialized, key_runtime_slot_interaction_t *out) {
    if (!out) {
        return;
    }

    if (!materialized) {
        *out = key_runtime_slot_interaction_default();
        return;
    }

    *out = (key_runtime_slot_interaction_t){
        .selection =
            {
                .keycode   = materialized->authored.keycode,
                .tap_count = materialized->authored.tap_count,
                .step      = materialized->authored.step,
            },
        .binding =
            {
                .tap_action             = materialized->tap_action,
                .tap_repeat_count       = materialized->tap_repeat_count,
                .hold                   = materialized->hold,
                .long_hold              = materialized->long_hold,
                .tap_hold_term          = materialized->authored.tap_hold_term,
                .longer_hold_term       = materialized->authored.longer_hold_term,
                .multi_tap_term         = materialized->authored.multi_tap_term,
                .branch_confirm_term    = materialized->authored.branch_confirm_term,
                .has_more_taps          = materialized->tap_has_more_taps,
                .authored_has_more_taps = materialized->authored.has_more_taps,
                .tap_resolves_on_press  = materialized->tap_resolves_on_press,
            },
        .hold_strategy = materialized->hold_strategy,
        .layer         = materialized->layer,
        .pd_mode       = materialized->pd_mode,
        .flags         = materialized->flags,
        .contract      = materialized->contract,
    };
}

static inline key_runtime_slot_interaction_t key_runtime_slot_interaction_from_materialized(handled_key_materialized_t materialized) {
    key_runtime_slot_interaction_t interaction;

    key_runtime_slot_interaction_from_materialized_into(&materialized, &interaction);
    return interaction;
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
