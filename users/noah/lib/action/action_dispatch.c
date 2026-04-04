// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#ifdef VIA_ENABLE
#    include "dynamic_keymap.h"
#endif

#include "noah_keymap.h"
#include "synthetic_record.h"
#include "../pointing/pointing_device_modes.h"
#include "../state/pd_shared_state.h"
#include "action_dispatch.h"

static uint8_t locked_layer = 0;

bool action_dispatch_is_layer_lock(uint16_t action) {
    return action >= LAYER_LOCK_BASE && action < LAYER_LOCK_BASE + LAYER_COUNT;
}

bool action_dispatch_is_macro(uint16_t action) {
    return (action >= MACRO_0 && action <= MACRO_15) || IS_QK_MACRO(action);
}

bool action_dispatch_layer_is_locked(uint8_t layer) {
    return locked_layer == layer;
}

void action_dispatch(uint16_t action) {
    if (action_dispatch_is_layer_lock(action)) {
        uint8_t layer = action - LAYER_LOCK_BASE;

        if (locked_layer == layer) {
            layer_off(layer);
            locked_layer = 0;
        } else {
            if (locked_layer) {
                layer_off(locked_layer);
            }
            layer_on(layer);
            locked_layer = layer;
        }
        return;
    }

    if (is_pd_mode_lock_action(action)) {
        const pd_mode_def_t *def = pd_mode_lock_action_lookup(action);
        if (def && pd_mode_toggle_lock_state(def->mode_flag)) {
            pd_shared_state_sync();
        }
        return;
    }

#ifdef VIA_ENABLE
    if (IS_QK_MACRO(action)) {
        dynamic_keymap_macro_send((uint8_t)(action - QK_MACRO));
        return;
    }
#endif

    if (macro_dispatch(action)) {
        return;
    }

    if (action >= NOAH_KEYMAP_SAFE_RANGE) {
        noah_dispatch_synthetic_tap(action);
        return;
    }

    tap_code16(action);
}
