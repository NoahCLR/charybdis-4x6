// ────────────────────────────────────────────────────────────────────────────
// Delayed Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "delayed_action.h"
#include "api.h"
#include "../../action/action_dispatch.h"
#include "../../state/modifiers/keyboard_mod_policy.h"

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    dispatch_delayed_action_at(NOAH_KEY_RUNTIME_NO_KEY, action, mods);
}

// Due fallback holds are settled before the replay suspends the live
// modifiers, so the state it restores still holds them while their keys are
// down; the tap goes out under them too.
void dispatch_delayed_action_at(keypos_t key_pos, uint16_t action, delayed_action_mods_t mods) {
    noah_emit_policy_t   policy = noah_action_keycode_is_macro(action) ? NOAH_EMIT_POLICY_PRESERVE_MODS : NOAH_EMIT_POLICY_NONE;
    uint8_t              before = keyboard_mod_policy_current_state().real;
    keyboard_mod_state_t saved;

    noah_key_runtime_settle_pending_fallback_hold(key_pos);
    saved = keyboard_mod_policy_begin_action_replay(keyboard_mod_policy_with_real_mods(mods, (uint8_t)(keyboard_mod_policy_current_state().real & ~before)));

    noah_emit_action_tap_at(key_pos, action, policy);

    keyboard_mod_policy_end_action_replay(action, saved);
}
