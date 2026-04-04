// ────────────────────────────────────────────────────────────────────────────
// Pointer Layer Policy
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "noah_keymap.h"
#include "pointing_device_modes.h"
#include "pointer_layer_policy.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)

static inline bool pointer_layer_policy_is_layer_hold_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

static inline bool pointer_layer_policy_pd_mode_running(void) {
    return pd_any_mode_active();
}

static bool pointer_layer_policy_is_pd_mode_key(uint16_t keycode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode) {
            return true;
        }
    }

    return false;
}

static inline bool pointer_layer_policy_auto_mouse_anchored(void) {
    return get_auto_mouse_toggle() || get_auto_mouse_key_tracker() != 0 || pointer_layer_policy_pd_mode_running();
}

bool pointer_layer_policy_is_mouse_record(uint16_t keycode) {
    if (pointer_layer_policy_pd_mode_running() && pointer_layer_policy_is_layer_hold_key(keycode)) {
        return true;
    }

    if (pointer_layer_policy_is_pd_mode_key(keycode)) {
        return true;
    }

    switch (keycode) {
        case DPI_MOD:
        case DPI_RMOD:
        case S_D_MOD:
        case S_D_RMOD:
            return true;
    }

    return false;
}

layer_state_t pointer_layer_policy_apply(layer_state_t state) {
    bool          pd_mode_running     = pointer_layer_policy_pd_mode_running();
    bool          auto_mouse_anchored = pointer_layer_policy_auto_mouse_anchored();
    uint8_t       auto_mouse_layer    = get_auto_mouse_layer();
    layer_state_t auto_mouse_mask     = (layer_state_t)1 << auto_mouse_layer;
    bool          auto_mouse_active   = layer_state_cmp(state, auto_mouse_layer);
    bool          nav_active          = layer_state_cmp(state, LAYER_NAV);

    // Keep the configured auto-mouse layer alive while anchored. Active pd
    // modes must survive even if QMK drops that layer underneath an LT-held
    // NAV key.
    if (pd_mode_running || (auto_mouse_anchored && (auto_mouse_layer == LAYER_NAV || !nav_active))) {
        state |= auto_mouse_mask;
        auto_mouse_active = true;
    }

    if (auto_mouse_layer != LAYER_NAV && auto_mouse_active && nav_active) {
        // NAV takes over from the configured auto-mouse layer, but not while a
        // pd mode is running.
        if (!auto_mouse_anchored) {
            state &= ~auto_mouse_mask;
        }
    } else if (auto_mouse_active) {
        bool other_layer_active = (state & ~auto_mouse_mask) != 0;
        if (other_layer_active && !auto_mouse_anchored) {
            state &= ~auto_mouse_mask;
        }
    }

    return state;
}

#else

bool pointer_layer_policy_is_mouse_record(uint16_t keycode) {
    (void)keycode;
    return false;
}

layer_state_t pointer_layer_policy_apply(layer_state_t state) {
    return state;
}

#endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE
