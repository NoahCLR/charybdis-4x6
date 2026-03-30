// ────────────────────────────────────────────────────────────────────────────
// Key Action Router
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "../../keymap_defs.h"
#include "../pointing/pointing_device_modes.h"
#include "../pointing/split_sync.h"
#include "action_router.h"

static uint8_t locked_layer = 0;

bool action_router_is_layer_lock_action(uint16_t action) {
    return action >= LAYER_LOCK_BASE && action < LAYER_LOCK_BASE + LAYER_COUNT;
}

bool action_router_layer_is_locked(uint8_t layer) {
    return locked_layer == layer;
}

void action_router_dispatch(uint16_t action) {
    if (action_router_is_layer_lock_action(action)) {
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
            pd_state_sync();
        }
        return;
    }

    tap_code16(action);
}
