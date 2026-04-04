// ────────────────────────────────────────────────────────────────────────────
// Delayed Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../action/action_dispatch.h"
#include "delayed_action.h"

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    keyboard_mod_state_t saved = keyboard_mod_state_suspend();

    keyboard_mod_state_apply(mods);

    action_dispatch(action);

    keyboard_mod_state_apply(saved);
}

void dispatch_multi_tap_action(uint16_t action, const multi_tap_t *mt) {
    dispatch_delayed_action(action, delayed_action_mods_from_multi_tap(mt));
}
