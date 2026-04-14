// ────────────────────────────────────────────────────────────────────────────
// Handled Key Lookup
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
