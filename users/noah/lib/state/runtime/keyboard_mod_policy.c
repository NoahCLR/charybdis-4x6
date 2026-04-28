// ────────────────────────────────────────────────────────────────────────────
// Keyboard Modifier Policy
// ────────────────────────────────────────────────────────────────────────────

#include "keyboard_mod_policy.h"

keyboard_mod_state_t keyboard_mod_policy_current_state(void) {
    return (keyboard_mod_state_t){
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };
}

keyboard_mod_state_t keyboard_mod_policy_without_mods(keyboard_mod_state_t state, uint8_t mods) {
    state.real &= (uint8_t)~mods;
    state.weak &= (uint8_t)~mods;
    state.oneshot &= (uint8_t)~mods;
    state.oneshot_locked &= (uint8_t)~mods;
    return state;
}

keyboard_mod_state_t keyboard_mod_policy_without_real_mods(keyboard_mod_state_t state, uint8_t mods) {
    state.real &= (uint8_t)~mods;
    return state;
}

keyboard_mod_state_t keyboard_mod_policy_with_real_mods(keyboard_mod_state_t state, uint8_t mods) {
    state.real |= mods;
    return state;
}

keyboard_mod_state_t keyboard_mod_policy_restore_after_action_replay(uint16_t action, keyboard_mod_state_t saved) {
    keyboard_mod_state_t emitted;

    if (!IS_QK_ONE_SHOT_MOD(action)) {
        return saved;
    }

    emitted = keyboard_mod_policy_current_state();

    // OSM()/other one-shot behavior keycodes intentionally leave state behind.
    // Preserve that result while restoring the real/weak mods suspended around
    // delayed replay.
    saved.oneshot |= emitted.oneshot;
    saved.oneshot_locked |= emitted.oneshot_locked;
    return saved;
}
