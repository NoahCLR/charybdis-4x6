// ────────────────────────────────────────────────────────────────────────────
// Delayed Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "delayed_action.h"
#include "../../action/action_dispatch.h"
#include "../../state/modifiers/keyboard_mod_policy.h"

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    dispatch_delayed_action_at((keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS}, action, mods);
}

void dispatch_delayed_action_at(keypos_t key_pos, uint16_t action, delayed_action_mods_t mods) {
    keyboard_mod_state_t saved = keyboard_mod_policy_begin_action_replay(mods);

    noah_emit_action_tap_at(key_pos, action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);

    keyboard_mod_policy_end_action_replay(action, saved);
}
