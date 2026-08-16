// ────────────────────────────────────────────────────────────────────────────
// Handled Key Resolution
// ────────────────────────────────────────────────────────────────────────────
//
// Runtime helpers that adapt authored key_behavior rows into a fully resolved
// handled-key contract for the press/release engine.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../pointing/defs/pd_mode_flags.h"
#include "../runtime/types.h"
#include "key_behavior.h"

typedef struct {
    uint16_t            keycode;
    uint8_t             tap_count;
    key_behavior_step_t step;
    uint16_t            tap_hold_term;
    uint16_t            longer_hold_term;
    uint16_t            multi_tap_term;
    uint16_t            branch_confirm_term;
    uint8_t             layer;
    pd_mode_mask_t      pd_mode;
    bool                has_more_taps;
    uint16_t            flags;
} handled_key_resolution_t;

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
    uint16_t                     threshold_action;
    uint16_t                     release_action;
    bool                         uses_held_lifecycle;
    bool                         keeps_registered_feedback;
    bool                         keeps_pending_feedback;
    bool                         release_layer_before_action;
    uint8_t                      preview_layer;
} handled_key_hold_semantics_t;

typedef handled_key_hold_semantics_t handled_key_hold_contract_t;

typedef struct {
    handled_key_hold_semantics_t hold;
    handled_key_hold_semantics_t long_hold;
    pd_mode_mask_t               quick_tap_pd_mode_lock;
    bool                         suppress_tap_on_layer_interrupt;
    bool                         buffered_base_tap_dispatches_tap;
    bool                         quick_release_of_immediate_hold_dispatches_tap;
    bool                         fallback_hold_suppresses_nonquick_release;
    bool                         nonquick_release_dispatches_tap;
} handled_key_behavior_contract_t;

typedef struct {
    layer_state_t active_layers;
    keypos_t      key_pos;
    int8_t        origin_layer;
} handled_key_resolution_ctx_t;

typedef struct {
    handled_key_resolution_t         authored;
    uint16_t                         tap_action;
    uint8_t                          tap_repeat_count;
    bool                             tap_has_more_taps;
    hold_behavior_t                  hold;
    hold_behavior_t                  long_hold;
    key_runtime_slot_hold_strategy_t hold_strategy;
    uint8_t                          layer;
    pd_mode_mask_t                   pd_mode;
    uint16_t                         flags;
    bool                             tap_resolves_on_press;
    handled_key_behavior_contract_t  contract;
} handled_key_materialized_t;

static inline handled_key_resolution_ctx_t handled_key_resolution_ctx_make(keypos_t key_pos, layer_state_t active_layers) {
    return (handled_key_resolution_ctx_t){
        .active_layers = active_layers,
        .key_pos       = key_pos,
        .origin_layer  = -1,
    };
}

static inline void handled_key_materialized_default_into(const handled_key_resolution_t *resolution, handled_key_materialized_t *out) {
    if (!out) {
        return;
    }

    *out = (handled_key_materialized_t){0};
    if (!resolution) {
        return;
    }

    out->authored = *resolution;
    out->layer    = resolution->layer;
    out->pd_mode  = resolution->pd_mode;
    out->flags    = resolution->flags;
}

static inline bool handled_key_hold_semantics_fires_at_threshold(handled_key_hold_semantics_t semantics) {
    return semantics.threshold != HANDLED_KEY_HOLD_THRESHOLD_NONE;
}

static inline bool handled_key_hold_semantics_registers_held(handled_key_hold_semantics_t semantics) {
    return semantics.threshold == HANDLED_KEY_HOLD_THRESHOLD_REGISTER_HELD;
}

static inline bool handled_key_hold_semantics_repeats_while_held(handled_key_hold_semantics_t semantics) {
    return semantics.threshold == HANDLED_KEY_HOLD_THRESHOLD_REPEAT;
}

static inline bool handled_key_hold_contract_fires_at_threshold(handled_key_hold_contract_t contract) {
    return handled_key_hold_semantics_fires_at_threshold(contract);
}

void                         handled_key_lookup_into(uint16_t keycode, handled_key_resolution_t *out);
void                         handled_key_lookup_tap_count_into(uint16_t keycode, uint8_t tap_count, handled_key_resolution_t *out);
handled_key_resolution_t     handled_key_lookup(uint16_t keycode);
handled_key_resolution_t     handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count);
handled_key_resolution_ctx_t handled_key_resolution_ctx_live(keypos_t key_pos);
__attribute__((noinline)) bool handled_key_resolution_materializes_momentary_layer(const handled_key_resolution_t *resolution, const handled_key_resolution_ctx_t *ctx);
void                         handled_key_materialize_into(const handled_key_resolution_t *resolution, const handled_key_resolution_ctx_t *ctx, handled_key_materialized_t *out);
handled_key_materialized_t   handled_key_materialize(handled_key_resolution_t resolution, handled_key_resolution_ctx_t ctx);
bool                         handled_key_resolution_is_handled(handled_key_resolution_t resolution);
bool                         handled_key_resolution_has_multi_tap(handled_key_resolution_t resolution);
bool                         handled_key_resolution_is_momentary_layer(handled_key_resolution_t resolution);
bool                         handled_key_resolution_is_layer_tap(handled_key_resolution_t resolution);
uint16_t                     handled_key_resolution_tap_hold_term(handled_key_resolution_t resolution);
uint16_t                     handled_key_resolution_longer_hold_term(handled_key_resolution_t resolution);
