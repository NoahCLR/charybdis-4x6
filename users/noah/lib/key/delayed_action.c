// ────────────────────────────────────────────────────────────────────────────
// Delayed Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "../action/action_dispatch.h"
#include "delayed_action.h"

typedef struct {
    uint8_t real;
    uint8_t weak;
    uint8_t oneshot;
    uint8_t oneshot_locked;
} delayed_action_saved_mods_t;

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
}

static delayed_action_saved_mods_t delayed_action_suspend_mods(void) {
    delayed_action_saved_mods_t saved = {
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };

    clear_mods();
    clear_weak_mods();
    clear_oneshot_mods();
    clear_oneshot_locked_mods();
    send_keyboard_report();

    return saved;
}

static void delayed_action_restore_mods(delayed_action_saved_mods_t saved) {
    set_mods(saved.real);
    set_weak_mods(saved.weak);
    set_oneshot_mods(saved.oneshot);
    set_oneshot_locked_mods(saved.oneshot_locked);
    send_keyboard_report();
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    delayed_action_saved_mods_t saved = delayed_action_suspend_mods();

    set_mods(mods.real);
    set_weak_mods(mods.weak);
    set_oneshot_mods(mods.oneshot);
    set_oneshot_locked_mods(mods.oneshot_locked);
    send_keyboard_report();

    action_dispatch(action);

    delayed_action_restore_mods(saved);
}

void dispatch_multi_tap_action(uint16_t action, const multi_tap_t *mt) {
    dispatch_delayed_action(action, delayed_action_mods_from_multi_tap(mt));
}
