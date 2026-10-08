// ────────────────────────────────────────────────────────────────────────────
// QMK One-Shot Contract
// ────────────────────────────────────────────────────────────────────────────
//
// OSL() is owned by userspace (layer_ownership_oneshot_*); QMK's own one-shot
// layer state is never set. QMK's keymap_config.oneshot_enable (Magic / VIA
// "one-shot keys") still decides whether a tap arms it: with one-shots off,
// OSL(layer) is only a hold, as QMK's is. ONESHOT_TIMEOUT and
// ONESHOT_TAP_TOGGLE apply to QMK's OSM() only.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

// Said at build time rather than silently ignored: someone coming from QMK
// sets these expecting them to apply to OSL() too.
#if defined(ONESHOT_TIMEOUT) && ONESHOT_TIMEOUT > 0
#    pragma message "ONESHOT_TIMEOUT applies to OSM() only: OSL() is owned by userspace and has no timeout"
#endif
#if defined(ONESHOT_TAP_TOGGLE) && ONESHOT_TAP_TOGGLE > 1
#    pragma message "ONESHOT_TAP_TOGGLE applies to OSM() only: OSL() is owned by userspace and does not lock on repeated taps"
#endif

static inline bool noah_qmk_contract_oneshot_enabled(void) {
#ifdef NO_ACTION_ONESHOT
    return false;
#else
    return is_oneshot_enabled();
#endif
}

// Apply QMK's modifier/one-shot exceptions to an already selected tap action.
// This does not decide whether a gesture tapped: its owner must decide first.
static inline bool noah_qmk_contract_tap_uses_oneshot_layer(uint16_t action) {
    if (IS_QK_MOD_TAP(action)) {
        action = QK_MOD_TAP_GET_TAP_KEYCODE(action);
    } else if (IS_QK_MODS(action)) {
        action = QK_MODS_GET_BASIC_KEYCODE(action);
    }
    return action != KC_NO && action != KC_TRNS && !IS_MODIFIER_KEYCODE(action) && !IS_QK_ONE_SHOT_MOD(action) && !IS_QK_ONE_SHOT_LAYER(action);
}
