// ────────────────────────────────────────────────────────────────────────────
// Handled Key Materialization
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key_internal.h"

handled_key_resolution_t handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count) {
    key_behavior_view_t behavior = key_behavior_lookup(keycode);
    key_behavior_step_t step     = tap_count <= 1 ? behavior.single : key_behavior_step_lookup(keycode, tap_count);
    bool                more     = key_behavior_has_more_taps(keycode, tap_count);

    return (handled_key_resolution_t){
        .keycode          = keycode,
        .tap_count        = tap_count,
        .step             = step,
        .tap_hold_term    = behavior.tap_hold_term,
        .longer_hold_term = behavior.longer_hold_term,
        .multi_tap_term   = behavior.multi_tap_term,
        .layer            = behavior.is_momentary_layer ? behavior_get_layer(behavior.keycode) : UINT8_MAX,
        .pd_mode          = handled_key_pd_mode_for_behavior(behavior),
        .has_more_taps    = more,
        .flags            = handled_key_flags_from_behavior(behavior),
    };
}

handled_key_resolution_t handled_key_lookup(uint16_t keycode) {
    return handled_key_lookup_tap_count(keycode, 1);
}

handled_key_resolution_ctx_t handled_key_resolution_ctx_live(keypos_t key_pos) {
    return handled_key_resolution_ctx_make(key_pos, layer_state | ((layer_state_t)1u << 0));
}

handled_key_materialized_t handled_key_materialize(handled_key_resolution_t resolution, handled_key_resolution_ctx_t ctx) {
    handled_key_transparent_source_t tap_source       = handled_key_transparent_source_at_position(resolution, ctx, HANDLED_KEY_TRANSPARENT_FIELD_TAP);
    handled_key_transparent_source_t hold_source      = handled_key_transparent_source_at_position(resolution, ctx, HANDLED_KEY_TRANSPARENT_FIELD_HOLD);
    handled_key_transparent_source_t long_hold_source = handled_key_transparent_source_at_position(resolution, ctx, HANDLED_KEY_TRANSPARENT_FIELD_LONG_HOLD);
    handled_key_materialized_t       materialized     = handled_key_materialized_default(resolution);

    if (tap_source.found) {
        materialized.tap_action       = handled_key_tap_action_behavior(tap_source.resolution);
        materialized.tap_repeat_count = handled_key_tap_repeat_count_behavior(tap_source.resolution, materialized.tap_action);
        materialized.tap_has_more_taps = tap_source.resolution.has_more_taps;
        materialized.tap_resolves_on_press = handled_key_tap_resolves_on_press_behavior(tap_source.resolution);
    }

    materialized.hold                  = hold_source.found ? handled_key_hold_behavior(hold_source.resolution) : hold_behavior_none();
    materialized.long_hold             = long_hold_source.found ? long_hold_source.resolution.step.long_hold : hold_behavior_none();
    materialized.hold_strategy         = hold_source.found ? handled_key_hold_strategy_behavior(hold_source.resolution) : KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
    materialized.layer                 = hold_source.found ? handled_key_resolution_source_layer(hold_source.resolution) : UINT8_MAX;
    materialized.pd_mode               = hold_source.found ? hold_source.resolution.pd_mode : 0;
    materialized.flags                 = resolution.flags & (uint16_t)~(HANDLED_KEY_FLAG_MOMENTARY_LAYER | HANDLED_KEY_FLAG_LAYER_TAP);

    if (hold_source.found && handled_key_resolution_source_is_momentary_layer(hold_source.resolution)) {
        materialized.flags |= HANDLED_KEY_FLAG_MOMENTARY_LAYER;
    }
    if (hold_source.found && (handled_key_resolution_source_is_layer_tap(hold_source.resolution) || handled_key_resolution_is_layer_tap(hold_source.resolution))) {
        materialized.flags |= HANDLED_KEY_FLAG_LAYER_TAP;
    }

    materialized.contract = handled_key_behavior_contract(materialized.hold_strategy, materialized.flags, materialized.tap_action, materialized.pd_mode, materialized.hold, materialized.long_hold);
    return materialized;
}
