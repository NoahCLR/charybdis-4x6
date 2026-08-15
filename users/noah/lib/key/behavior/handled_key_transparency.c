// ────────────────────────────────────────────────────────────────────────────
// Handled Key Transparency
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key_internal.h"

#include "keymap_introspection.h" // QMK

static bool handled_key_keypos_in_bounds(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static uint16_t handled_key_resolution_transparent_field_action(const handled_key_resolution_t *resolution, handled_key_transparent_field_t field) {
    if (!resolution) {
        return KC_NO;
    }

    switch (field) {
        case HANDLED_KEY_TRANSPARENT_FIELD_TAP:
            return resolution->step.tap.present ? resolution->step.tap.action : noah_action_desc_default_tap_action(noah_action_describe(resolution->keycode));
        case HANDLED_KEY_TRANSPARENT_FIELD_HOLD:
            return resolution->step.hold.present ? resolution->step.hold.action : KC_NO;
        case HANDLED_KEY_TRANSPARENT_FIELD_LONG_HOLD:
            return resolution->step.long_hold.present ? resolution->step.long_hold.action : KC_NO;
        default:
            return KC_NO;
    }
}

bool handled_key_resolution_uses_transparent_source(const handled_key_resolution_t *resolution, handled_key_transparent_field_t field) {
    return handled_key_resolution_transparent_field_action(resolution, field) == KC_TRNS;
}

static noah_action_desc_t handled_key_resolution_action_desc(const handled_key_resolution_t *resolution) {
    return noah_action_describe(resolution ? resolution->keycode : KC_NO);
}

bool handled_key_resolution_source_is_layer_tap(const handled_key_resolution_t *resolution) {
    return resolution && (handled_key_resolution_is_layer_tap(*resolution) || noah_action_desc_source_sets_layer_tap_flag(handled_key_resolution_action_desc(resolution)));
}

bool handled_key_resolution_source_is_momentary_layer(const handled_key_resolution_t *resolution) {
    return resolution && (handled_key_resolution_is_momentary_layer(*resolution) || noah_action_desc_source_sets_momentary_layer_flag(handled_key_resolution_action_desc(resolution)));
}

uint8_t handled_key_resolution_source_layer(const handled_key_resolution_t *resolution) {
    if (!resolution) {
        return UINT8_MAX;
    }

    if (handled_key_resolution_is_momentary_layer(*resolution)) {
        return resolution->layer;
    }

    return noah_action_desc_source_layer(handled_key_resolution_action_desc(resolution));
}

static int8_t handled_key_transparent_origin_layer(const handled_key_resolution_t *resolution, const handled_key_resolution_ctx_t *ctx) {
    if (!(resolution && ctx)) {
        return -1;
    }

    if (ctx->origin_layer >= 0) {
        return ctx->origin_layer;
    }

    if (!handled_key_keypos_in_bounds(ctx->key_pos)) {
        return -1;
    }

    for (int8_t layer = (int8_t)(LAYER_COUNT - 1); layer >= 0; layer--) {
        if (!layer_state_cmp(ctx->active_layers, (uint8_t)layer)) {
            continue;
        }

        if (keycode_at_keymap_location((uint8_t)layer, ctx->key_pos.row, ctx->key_pos.col) == resolution->keycode) {
            return layer;
        }
    }

    return -1;
}

bool handled_key_transparent_source_at_position(const handled_key_resolution_t *resolution, const handled_key_resolution_ctx_t *ctx, handled_key_transparent_field_t field, handled_key_resolution_t *out) {
    int8_t origin_layer;

    if (!(resolution && ctx && out)) {
        return false;
    }

    if (!handled_key_resolution_uses_transparent_source(resolution, field)) {
        *out = *resolution;
        return true;
    }

    origin_layer = handled_key_transparent_origin_layer(resolution, ctx);
    if (origin_layer <= 0) {
        return false;
    }

    for (int8_t layer = (int8_t)(origin_layer - 1); layer >= 0; layer--) {
        uint16_t keycode;

        if (!layer_state_cmp(ctx->active_layers, (uint8_t)layer)) {
            continue;
        }

        keycode = keycode_at_keymap_location((uint8_t)layer, ctx->key_pos.row, ctx->key_pos.col);
        if (keycode == KC_TRNS) {
            continue;
        }

        handled_key_lookup_tap_count_into(keycode, resolution->tap_count, out);
        if (handled_key_resolution_uses_transparent_source(out, field)) {
            continue;
        }

        return true;
    }

    return false;
}
