// ────────────────────────────────────────────────────────────────────────────
// Handled Key Resolution Accessors
// ────────────────────────────────────────────────────────────────────────────

#include "handled_key_internal.h"

bool handled_key_resolution_is_handled(handled_key_resolution_t resolution) {
    return (resolution.flags & HANDLED_KEY_FLAG_HANDLED) != 0;
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

uint16_t handled_key_resolution_tap_hold_term(handled_key_resolution_t resolution) {
    return resolution.tap_hold_term;
}

uint16_t handled_key_resolution_longer_hold_term(handled_key_resolution_t resolution) {
    return resolution.longer_hold_term;
}
