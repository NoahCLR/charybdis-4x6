// ────────────────────────────────────────────────────────────────────────────
// Keyboard Modifier State
// ────────────────────────────────────────────────────────────────────────────

#include "keyboard_mod_state.h"

keyboard_mod_state_t keyboard_mod_state_suspend(void) {
    keyboard_mod_state_t saved = {
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

void keyboard_mod_state_apply(keyboard_mod_state_t state) {
    set_mods(state.real);
    set_weak_mods(state.weak);
    set_oneshot_mods(state.oneshot);
    set_oneshot_locked_mods(state.oneshot_locked);
    send_keyboard_report();
}
