// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "action_lifecycle.h"
#include "owned_keycode.h"
#include "synthetic_record.h"
#include "../key/runtime/api.h"
#include "../state/modifiers/keyboard_mod_policy.h"
#include "action_dispatch.h"

typedef void (*noah_emit_tap_fn_t)(uint16_t keycode);
typedef void (*noah_emit_tap_at_fn_t)(keypos_t key_pos, uint16_t keycode);

static void noah_emit_run(uint16_t keycode, noah_emit_tap_fn_t emit, noah_emit_policy_t policy) {
    keyboard_mod_state_t saved_mod_state = {0};

    if (policy.settle_pending_fallback_holds) {
        noah_key_runtime_settle_pending_fallback_hold();
    }

    if (policy.preserve_keyboard_mod_state) {
        saved_mod_state = keyboard_mod_policy_begin_preserve_all();
    }

    emit(keycode);

    if (policy.preserve_keyboard_mod_state) {
        keyboard_mod_policy_end_preserve_all(saved_mod_state);
    }
}

static void noah_emit_run_at(keypos_t key_pos, uint16_t keycode, noah_emit_tap_at_fn_t emit, noah_emit_policy_t policy) {
    keyboard_mod_state_t saved_mod_state = {0};

    if (policy.settle_pending_fallback_holds) {
        noah_key_runtime_settle_pending_fallback_hold();
    }

    if (policy.preserve_keyboard_mod_state) {
        saved_mod_state = keyboard_mod_policy_begin_preserve_all();
    }

    emit(key_pos, keycode);

    if (policy.preserve_keyboard_mod_state) {
        keyboard_mod_policy_end_preserve_all(saved_mod_state);
    }
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    noah_emit_run(action, noah_action_tap, policy);
}

__attribute__((noinline)) void noah_emit_action_tap_at(keypos_t key_pos, uint16_t action, noah_emit_policy_t policy) {
    noah_emit_run_at(key_pos, action, noah_action_tap_at, policy);
}

void noah_emit_synthetic_qmk_tap(uint16_t keycode, noah_emit_policy_t policy) {
    noah_emit_run(keycode, noah_dispatch_synthetic_qmk_tap, policy);
}

void noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(uint16_t keycode, uint8_t masked_mods, bool settle_pending_fallback_holds) {
    if (settle_pending_fallback_holds) {
        noah_key_runtime_settle_pending_fallback_hold();
    }

    keyboard_mod_state_t saved_mod_state = keyboard_mod_policy_begin_masked_emit(masked_mods);

    noah_dispatch_synthetic_qmk_tap(keycode);
    keyboard_mod_policy_end_masked_emit(saved_mod_state);
}

void noah_emit_literal_tap(uint16_t keycode, noah_emit_policy_t policy) {
    noah_emit_run(keycode, owned_keycode_tap_literal, policy);
}

void noah_emit_literal_tap_with_masked_keyboard_mods(uint16_t keycode, uint8_t masked_mods, bool settle_pending_fallback_holds) {
    if (settle_pending_fallback_holds) {
        noah_key_runtime_settle_pending_fallback_hold();
    }
    keyboard_mod_state_t saved = keyboard_mod_policy_begin_masked_emit(masked_mods);
    owned_keycode_tap_literal(keycode);
    keyboard_mod_policy_end_masked_emit(saved);
}

static uint8_t noah_emit_host_mods(keyboard_mod_state_t state) {
    return state.real | state.weak | state.oneshot | state.oneshot_locked;
}

// The button leaves on the mouse endpoint, which drains apart from the
// keyboard's, so a modifier change sent just before it can still reach the
// host after it (see owned_keycode_acquire_components()). When hiding
// modifiers changes what the host sees, the button waits for that report and
// the modifiers come back only once the button is up.
void noah_emit_owned_click(uint16_t keycode, uint8_t masked_mods, bool exact) {
    noah_key_runtime_settle_pending_fallback_hold();

    uint8_t              host_mods = noah_emit_host_mods(keyboard_mod_policy_current_state());
    keyboard_mod_state_t saved     = exact ? keyboard_mod_policy_begin_preserve_all() : keyboard_mod_policy_begin_masked_emit(masked_mods);
    bool                 changed   = noah_emit_host_mods(keyboard_mod_policy_current_state()) != host_mods;

    if (changed) {
        wait_ms(OWNED_KEYCODE_MOD_TO_MOUSE_SETTLE_MS);
    }
    owned_keycode_tap_literal(keycode);
    if (changed) {
        wait_ms(OWNED_KEYCODE_MOD_TO_MOUSE_SETTLE_MS);
    }

    if (exact) {
        keyboard_mod_policy_end_preserve_all(saved);
    } else {
        keyboard_mod_policy_end_masked_emit(saved);
    }
}

void action_dispatch(uint16_t action) {
    noah_emit_action_tap(action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
}
