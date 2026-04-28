// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "action_lifecycle.h"
#include "synthetic_record.h"
#include "../key/runtime/api.h"
#include "../state/runtime/keyboard_mod_policy.h"
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

void noah_emit_action_tap_at(keypos_t key_pos, uint16_t action, noah_emit_policy_t policy) {
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
    noah_emit_run(keycode, tap_code16, policy);
}

void action_dispatch(uint16_t action) {
    noah_emit_action_tap(action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
}
