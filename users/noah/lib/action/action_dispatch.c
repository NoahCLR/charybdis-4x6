// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "action_lifecycle.h"
#include "synthetic_record.h"
#include "../key/runtime/key_runtime_api.h"
#include "../state/runtime/keyboard_mod_state.h"
#include "action_dispatch.h"

typedef void (*noah_emit_tap_fn_t)(uint16_t keycode);

static keyboard_mod_state_t keyboard_mod_state_without_mods(keyboard_mod_state_t state, uint8_t mods) {
    state.real &= (uint8_t)~mods;
    state.weak &= (uint8_t)~mods;
    state.oneshot &= (uint8_t)~mods;
    state.oneshot_locked &= (uint8_t)~mods;
    return state;
}

static void noah_emit_run(uint16_t keycode, noah_emit_tap_fn_t emit, noah_emit_policy_t policy) {
    keyboard_mod_state_t saved_mod_state = {0};

    if (policy.settle_pending_fallback_holds) {
        noah_key_runtime_settle_pending_fallback_hold();
    }

    if (policy.preserve_keyboard_mod_state) {
        saved_mod_state = keyboard_mod_state_suspend();
    }

    emit(keycode);

    if (policy.preserve_keyboard_mod_state) {
        keyboard_mod_state_apply(saved_mod_state);
    }
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    noah_emit_run(action, noah_action_tap, policy);
}

void noah_emit_synthetic_qmk_tap(uint16_t keycode, noah_emit_policy_t policy) {
    noah_emit_run(keycode, noah_dispatch_synthetic_qmk_tap, policy);
}

void noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(uint16_t keycode, uint8_t masked_mods, bool settle_pending_fallback_holds) {
    if (settle_pending_fallback_holds) {
        noah_key_runtime_settle_pending_fallback_hold();
    }

    keyboard_mod_state_t saved_mod_state = {
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };
    keyboard_mod_state_t filtered_mod_state = keyboard_mod_state_without_mods(saved_mod_state, masked_mods);

    keyboard_mod_state_apply(filtered_mod_state);
    noah_dispatch_synthetic_qmk_tap(keycode);
    keyboard_mod_state_apply(saved_mod_state);
}

void noah_emit_literal_tap(uint16_t keycode, noah_emit_policy_t policy) {
    noah_emit_run(keycode, tap_code16, policy);
}

void action_dispatch(uint16_t action) {
    noah_emit_action_tap(action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
}
