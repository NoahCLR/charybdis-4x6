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
    noah_emit_policy_t   policy = noah_action_keycode_is_macro(action) ? NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS : NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS;
    keyboard_mod_state_t saved  = keyboard_mod_policy_begin_action_replay(mods);

    noah_emit_action_tap_at(key_pos, action, policy);

    keyboard_mod_policy_end_action_replay(action, saved);
}
