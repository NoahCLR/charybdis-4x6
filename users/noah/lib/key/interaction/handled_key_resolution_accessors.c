// ────────────────────────────────────────────────────────────────────────────
// Handled Key Resolution Accessors
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key_internal.h"

bool handled_key_resolution_is_handled(handled_key_resolution_t resolution) {
    return (resolution.flags & HANDLED_KEY_FLAG_HANDLED) != 0;
}

bool handled_key_resolution_uses_implicit_hold(handled_key_resolution_t resolution) {
    return resolution.tap_count == 1 && resolution.pd_mode != 0;
}

bool handled_key_resolution_uses_fallback_hold(handled_key_resolution_t resolution) {
    return handled_key_resolution_uses_fallback_hold_behavior(resolution);
}

bool handled_key_resolution_has_multi_tap(handled_key_resolution_t resolution) {
    return (resolution.flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0;
}

bool handled_key_resolution_is_momentary_layer(handled_key_resolution_t resolution) {
    return (resolution.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0;
}

bool handled_key_resolution_is_layer_tap(handled_key_resolution_t resolution) {
    return (resolution.flags & HANDLED_KEY_FLAG_LAYER_TAP) != 0;
}

hold_behavior_t handled_key_resolution_hold(handled_key_resolution_t resolution) {
    return handled_key_hold_behavior(resolution);
}

hold_behavior_t handled_key_resolution_long_hold(handled_key_resolution_t resolution) {
    return resolution.step.long_hold;
}

key_runtime_slot_hold_strategy_t handled_key_resolution_hold_strategy(handled_key_resolution_t resolution) {
    return handled_key_hold_strategy_behavior(resolution);
}

uint16_t handled_key_resolution_tap_action(handled_key_resolution_t resolution) {
    return handled_key_tap_action_behavior(resolution);
}

uint8_t handled_key_resolution_tap_repeat_count(handled_key_resolution_t resolution) {
    return handled_key_tap_repeat_count_behavior(resolution, handled_key_resolution_tap_action(resolution));
}

bool handled_key_resolution_tap_resolves_on_press(handled_key_resolution_t resolution) {
    return handled_key_tap_resolves_on_press_behavior(resolution);
}

uint16_t handled_key_resolution_tap_hold_term(handled_key_resolution_t resolution) {
    return resolution.tap_hold_term;
}

uint16_t handled_key_resolution_longer_hold_term(handled_key_resolution_t resolution) {
    return resolution.longer_hold_term;
}

uint16_t handled_key_resolution_multi_tap_term(handled_key_resolution_t resolution) {
    return resolution.multi_tap_term;
}

uint8_t handled_key_resolution_layer(handled_key_resolution_t resolution) {
    return resolution.layer;
}

pd_mode_mask_t handled_key_resolution_pd_mode(handled_key_resolution_t resolution) {
    return resolution.pd_mode;
}
