// ────────────────────────────────────────────────────────────────────────────
// Pointer Layer Policy
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "pd_modes.h"
#include "pointer_layer_policy.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)

static inline bool pointer_layer_policy_is_layer_hold_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

static inline bool pointer_layer_policy_pd_mode_running(void) {
    return pd_any_mode_active();
}

static inline bool pointer_layer_policy_arrow_mode_prefers_typing_layer(void) {
    return pd_mode_active(PD_MODE_ARROW);
}

static inline bool pointer_layer_policy_pd_mode_keeps_auto_mouse_anchored(void) {
    return pointer_layer_policy_pd_mode_running() && !pointer_layer_policy_arrow_mode_prefers_typing_layer();
}

static bool pointer_layer_policy_pd_mode_key_keeps_auto_mouse_anchored(uint16_t keycode) {
    pd_mode_mask_t mode = pd_mode_for_keycode(keycode);
    return mode != 0 && mode != PD_MODE_ARROW;
}

static inline bool pointer_layer_policy_auto_mouse_anchored(void) {
    return get_auto_mouse_toggle() || get_auto_mouse_key_tracker() != 0 || pointer_layer_policy_pd_mode_keeps_auto_mouse_anchored();
}

bool pointer_layer_policy_is_mouse_record(uint16_t keycode) {
    if (pointer_layer_policy_pd_mode_keeps_auto_mouse_anchored() && pointer_layer_policy_is_layer_hold_key(keycode)) {
        return true;
    }

    if (pointer_layer_policy_pd_mode_key_keeps_auto_mouse_anchored(keycode)) {
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
    bool          arrow_mode_active   = pointer_layer_policy_arrow_mode_prefers_typing_layer();
    bool          auto_mouse_anchored = pointer_layer_policy_auto_mouse_anchored();
    uint8_t       auto_mouse_layer    = get_auto_mouse_layer();
    layer_state_t auto_mouse_mask     = (layer_state_t)1 << auto_mouse_layer;
    bool          auto_mouse_active   = layer_state_cmp(state, auto_mouse_layer);
    bool          nav_active          = layer_state_cmp(state, CHARYBDIS_AUTO_SNIPING_LAYER);

    // Arrow mode consumes trackball motion as arrows, so keep the keyboard on
    // the current typing/nav surface instead of forcing the pointer layer back
    // underneath it. Holding NAV can still expose the pointer-button layout.
    if (arrow_mode_active && auto_mouse_layer != CHARYBDIS_AUTO_SNIPING_LAYER) {
        state &= ~auto_mouse_mask;
        return state;
    }

    // Keep the configured auto-mouse layer alive while anchored. Active pd
    // modes must survive even if QMK drops that layer underneath an LT-held
    // NAV key.
    if (auto_mouse_anchored && (auto_mouse_layer == CHARYBDIS_AUTO_SNIPING_LAYER || !nav_active)) {
        state |= auto_mouse_mask;
        auto_mouse_active = true;
    }

    if (auto_mouse_layer != CHARYBDIS_AUTO_SNIPING_LAYER && auto_mouse_active && nav_active) {
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
