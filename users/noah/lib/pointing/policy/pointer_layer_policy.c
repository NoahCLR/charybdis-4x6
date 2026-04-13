// ────────────────────────────────────────────────────────────────────────────
// Pointer Layer Policy
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "pd_mode_policy.h"
#include "../defs/pd_modes.h"
#include "pointer_layer_policy.h"
#include "../../compat/qmk_auto_mouse_contract.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
static inline bool pointer_layer_policy_is_layer_hold_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

static inline bool pointer_layer_policy_is_mouse_button_action(uint16_t action) {
    return IS_MOUSEKEY_BUTTON(action);
}

static bool pointer_layer_policy_pd_mode_key_keeps_auto_mouse_anchored(uint16_t keycode) {
    return pd_mode_policy_key_keeps_auto_mouse_anchored(keycode);
}

static inline bool pointer_layer_policy_auto_mouse_anchored(pd_mode_snapshot_t snapshot) {
    return noah_qmk_contract_auto_mouse_toggle_enabled() || noah_qmk_contract_auto_mouse_key_tracker() != 0 || pd_mode_policy_snapshot_keeps_auto_mouse_anchored(snapshot);
}

bool pointer_layer_policy_is_mouse_record(uint16_t keycode) {
    pd_mode_snapshot_t snapshot = pd_mode_snapshot();

    if (pd_mode_policy_snapshot_keeps_auto_mouse_anchored(snapshot) && pointer_layer_policy_is_layer_hold_key(keycode)) {
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

bool pointer_layer_policy_is_mouse_action(uint16_t action) {
    return pointer_layer_policy_is_mouse_button_action(action);
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    if (!pointer_layer_policy_is_mouse_action(action)) {
        return;
    }

    noah_qmk_contract_auto_mouse_keyevent(pressed);
}

layer_state_t pointer_layer_policy_apply(layer_state_t state) {
    pd_mode_snapshot_t snapshot             = pd_mode_snapshot();
    bool               prefers_typing_layer = pd_mode_policy_snapshot_prefers_typing_layer(snapshot);
    bool               auto_mouse_anchored  = pointer_layer_policy_auto_mouse_anchored(snapshot);
    uint8_t            auto_mouse_layer     = noah_qmk_contract_auto_mouse_layer();
    layer_state_t      auto_mouse_mask      = (layer_state_t)1 << auto_mouse_layer;
    bool               sniping_layer_active = layer_state_cmp(state, CHARYBDIS_AUTO_SNIPING_LAYER);

    // Arrow mode consumes trackball motion as arrows, so keep the keyboard on
    // the current typing/nav surface instead of forcing the pointer layer back
    // underneath it. Holding NAV can still expose the pointer-button layout.
    if (prefers_typing_layer && auto_mouse_layer != CHARYBDIS_AUTO_SNIPING_LAYER) {
        state &= ~auto_mouse_mask;
        return state;
    }

    // Auto-sniping must stay authoritative while mousing. If the sniping
    // layer is active and auto-mouse targets some other layer, strip that
    // separate pointer layer regardless of anchors so NAV keeps keymap
    // precedence and sniping stays enabled.
    if (sniping_layer_active && auto_mouse_layer != CHARYBDIS_AUTO_SNIPING_LAYER) {
        state &= ~auto_mouse_mask;
        return state;
    }

    // Keep the configured auto-mouse layer alive while anchored. Outside the
    // sniping exception above, overlap with other keyboard layers is allowed.
    if (auto_mouse_anchored) {
        state |= auto_mouse_mask;
    }

    return state;
}

#else

bool pointer_layer_policy_is_mouse_record(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool pointer_layer_policy_is_mouse_action(uint16_t action) {
    (void)action;
    return false;
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

layer_state_t pointer_layer_policy_apply(layer_state_t state) {
    return state;
}

#endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE
