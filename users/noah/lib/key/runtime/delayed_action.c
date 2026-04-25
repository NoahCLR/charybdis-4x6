// ────────────────────────────────────────────────────────────────────────────
// Delayed Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "delayed_action.h"
#include "../../action/action_dispatch.h"

static keyboard_mod_state_t delayed_action_current_mod_state(void) {
    return (keyboard_mod_state_t){
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };
}

static keyboard_mod_state_t delayed_action_restore_mod_state(uint16_t action, keyboard_mod_state_t saved) {
    if (!IS_QK_ONE_SHOT_MOD(action)) {
        return saved;
    }

    keyboard_mod_state_t emitted = delayed_action_current_mod_state();

    // OSM()/other one-shot behavior keycodes intentionally leave state behind.
    // Preserve that result while restoring the real/weak mods suspended around
    // delayed replay.
    saved.oneshot |= emitted.oneshot;
    saved.oneshot_locked |= emitted.oneshot_locked;
    return saved;
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    dispatch_delayed_action_at((keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS}, action, mods);
}

void dispatch_delayed_action_at(keypos_t key_pos, uint16_t action, delayed_action_mods_t mods) {
    keyboard_mod_state_t saved = keyboard_mod_state_suspend();

    keyboard_mod_state_apply(mods);

    noah_emit_action_tap_at(key_pos, action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);

    keyboard_mod_state_apply(delayed_action_restore_mod_state(action, saved));
}
