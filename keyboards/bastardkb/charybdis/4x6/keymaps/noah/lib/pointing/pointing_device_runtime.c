// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Runtime
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "../../keymap_defs.h"
#include "pointing_device_modes.h"
#include "split_sync.h"

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)
#endif

#ifdef POINTING_DEVICE_ENABLE

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
static inline bool is_layer_hold_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

void pointing_device_init_user(void) {
    set_auto_mouse_layer(LAYER_POINTER);
    set_auto_mouse_enable(true);
    split_sync_init();
}

bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)record;

    // While a pointing-device mode is active, layer keys like MO()/LT()
    // must not make auto-mouse drop LAYER_POINTER underneath held mouse keys.
    if ((pd_mode_flags != 0 || pd_mode_locked_flags != 0) && is_layer_hold_key(keycode)) {
        return true;
    }

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode) return true;
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
#    endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_modes[i].mode_flag) && pd_modes[i].handler) {
            return pd_modes[i].handler(mouse_report);
        }
    }
    return mouse_report;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    charybdis_set_pointer_sniping_enabled(layer_state_cmp(state, LAYER_RAISE));

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    // Anchored = auto-mouse should be kept alive: toggle on, a mouse-record key is held,
    // or a pointing-device mode is active or locked.
    bool pd_mode_running    = pd_mode_flags != 0 || pd_mode_locked_flags != 0;
    bool auto_mouse_anchored = get_auto_mouse_toggle() || get_auto_mouse_key_tracker() != 0 || pd_mode_running;

    // Keep LAYER_POINTER alive while anchored. Active pd modes must survive
    // even if QMK drops POINTER underneath an LT-held RAISE key.
    if (pd_mode_running || (auto_mouse_anchored && !layer_state_cmp(state, LAYER_RAISE))) {
        state |= (layer_state_t)1 << LAYER_POINTER;
    }

    if (layer_state_cmp(state, LAYER_POINTER) && layer_state_cmp(state, LAYER_RAISE)) {
        // RAISE takes over from POINTER, but not while a pd mode is running.
        if (!auto_mouse_anchored) {
            state &= ~((layer_state_t)1 << LAYER_POINTER);
        }
    } else if (layer_state_cmp(state, LAYER_POINTER)) {
        bool other_layer_active = (state & ~((layer_state_t)1 << LAYER_POINTER)) != 0;
        if (other_layer_active && !auto_mouse_anchored) {
            state &= ~((layer_state_t)1 << LAYER_POINTER);
        }
    }
#    endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

    return state;
}

#endif // POINTING_DEVICE_ENABLE
