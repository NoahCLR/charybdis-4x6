// ────────────────────────────────────────────────────────────────────────────
// Handled Key Materialization
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key_internal.h"

__attribute__((noinline)) bool handled_key_resolution_materializes_momentary_layer(const handled_key_resolution_t *resolution, const handled_key_resolution_ctx_t *ctx) {
    handled_key_resolution_t source;

    return handled_key_transparent_source_at_position(resolution, ctx, HANDLED_KEY_TRANSPARENT_FIELD_HOLD, &source) && handled_key_resolution_source_is_momentary_layer(&source);
}

static __attribute__((noinline)) void handled_key_materialize_tap(const handled_key_resolution_t *resolution, const handled_key_resolution_ctx_t *ctx, handled_key_materialized_t *out) {
    handled_key_resolution_t source;

    if (!handled_key_transparent_source_at_position(resolution, ctx, HANDLED_KEY_TRANSPARENT_FIELD_TAP, &source)) {
        return;
    }

    out->tap_action            = handled_key_tap_action_behavior(&source);
    out->tap_repeat_count      = handled_key_tap_repeat_count_behavior(&source, out->tap_action);
    out->tap_has_more_taps     = source.has_more_taps;
    out->tap_resolves_on_press = handled_key_tap_resolves_on_press_behavior(&source);
}

static __attribute__((noinline)) void handled_key_materialize_hold(const handled_key_resolution_t *resolution, const handled_key_resolution_ctx_t *ctx, handled_key_materialized_t *out) {
    handled_key_resolution_t source;
    bool                     source_found;

    source_found       = handled_key_transparent_source_at_position(resolution, ctx, HANDLED_KEY_TRANSPARENT_FIELD_HOLD, &source);
    out->hold          = source_found ? handled_key_hold_behavior(&source) : hold_behavior_none();
    out->hold_strategy = source_found ? handled_key_hold_strategy_behavior(&source) : KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
    out->layer         = source_found ? handled_key_resolution_source_layer(&source) : UINT8_MAX;
    out->pd_mode       = source_found ? source.pd_mode : 0;
    out->flags         = resolution->flags & (uint16_t)~(HANDLED_KEY_FLAG_MOMENTARY_LAYER | HANDLED_KEY_FLAG_LAYER_TAP);

    if (source_found && handled_key_resolution_source_is_momentary_layer(&source)) {
        out->flags |= HANDLED_KEY_FLAG_MOMENTARY_LAYER;
    }
    if (source_found && handled_key_resolution_source_is_layer_tap(&source)) {
        out->flags |= HANDLED_KEY_FLAG_LAYER_TAP;
    }
}

static __attribute__((noinline)) void handled_key_materialize_long_hold(const handled_key_resolution_t *resolution, const handled_key_resolution_ctx_t *ctx, handled_key_materialized_t *out) {
    handled_key_resolution_t source;

    out->long_hold = handled_key_transparent_source_at_position(resolution, ctx, HANDLED_KEY_TRANSPARENT_FIELD_LONG_HOLD, &source) ? source.step.long_hold : hold_behavior_none();
}

void handled_key_materialized_refresh_contract(handled_key_materialized_t *materialized) {
    if (!materialized) {
        return;
    }

    materialized->flags &= (uint16_t)~(HANDLED_KEY_FLAG_IMPLICIT_HOLD | HANDLED_KEY_FLAG_FALLBACK_HOLD);

    if (materialized->hold_strategy == KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT) {
        materialized->flags |= HANDLED_KEY_FLAG_IMPLICIT_HOLD;
    }
    if (materialized->hold_strategy == KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK) {
        materialized->flags |= HANDLED_KEY_FLAG_FALLBACK_HOLD;
    }

    materialized->contract = handled_key_behavior_contract(materialized->hold_strategy, materialized->flags, materialized->tap_action, materialized->pd_mode, materialized->hold, materialized->long_hold);
}

void handled_key_materialize_into(const handled_key_resolution_t *resolution, const handled_key_resolution_ctx_t *ctx, handled_key_materialized_t *out) {
    if (!(resolution && ctx && out)) {
        return;
    }

    handled_key_materialized_default_into(resolution, out);
    handled_key_materialize_tap(resolution, ctx, out);
    handled_key_materialize_hold(resolution, ctx, out);
    handled_key_materialize_long_hold(resolution, ctx, out);

    handled_key_materialized_refresh_contract(out);
}

handled_key_materialized_t handled_key_materialize(handled_key_resolution_t resolution, handled_key_resolution_ctx_t ctx) {
    handled_key_materialized_t materialized;

    handled_key_materialize_into(&resolution, &ctx, &materialized);
    return materialized;
}
