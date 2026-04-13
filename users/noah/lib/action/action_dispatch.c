// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "noah_keymap_ids.h"
#include "action_lifecycle.h"
#include "synthetic_record.h"
#include "../key/runtime/key_runtime_state.h"
#include "../state/runtime/keyboard_mod_state.h"
#include "../state/ownership/layer_ownership.h"
#include "action_dispatch.h"

typedef void (*noah_emit_tap_fn_t)(uint16_t keycode);

static void noah_emit_run(uint16_t keycode, noah_emit_tap_fn_t emit, noah_emit_policy_t policy) {
    keyboard_mod_state_t saved_mod_state = {0};

    if (policy.settle_pending_fallback_holds) {
        key_runtime_activate_pending_fallback_hold();
    }

    if (policy.preserve_keyboard_mod_state) {
        saved_mod_state = keyboard_mod_state_suspend();
    }

    emit(keycode);

    if (policy.preserve_keyboard_mod_state) {
        keyboard_mod_state_apply(saved_mod_state);
    }
}

bool action_dispatch_is_layer_lock(uint16_t action) {
    return action >= LAYER_LOCK_BASE && action < LAYER_LOCK_BASE + LAYER_COUNT;
}

bool action_dispatch_is_raw_qmk_layer_action(uint16_t action) {
    return IS_QK_TO(action) || IS_QK_MOMENTARY(action) || IS_QK_DEF_LAYER(action) || IS_QK_TOGGLE_LAYER(action) || IS_QK_ONE_SHOT_LAYER(action) || IS_QK_LAYER_TAP_TOGGLE(action) || IS_QK_LAYER_MOD(action) || IS_QK_LAYER_TAP(action);
}

bool action_dispatch_is_layer_action(uint16_t action) {
    return action_dispatch_is_layer_lock(action) || action_dispatch_is_raw_qmk_layer_action(action);
}

bool action_dispatch_is_macro(uint16_t action) {
    return (action >= MACRO_0 && action <= MACRO_15) || IS_QK_MACRO(action);
}

bool action_dispatch_is_qmk_behavior_keycode(uint16_t action) {
    return IS_QK_ONE_SHOT_MOD(action) || IS_QK_MOD_TAP(action);
}

bool action_dispatch_layer_is_locked(uint8_t layer) {
    return layer_ownership_is_locked(layer);
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    noah_emit_run(action, noah_action_tap, policy);
}

void noah_emit_synthetic_qmk_tap(uint16_t keycode, noah_emit_policy_t policy) {
    noah_emit_run(keycode, noah_dispatch_synthetic_qmk_tap, policy);
}

void noah_emit_literal_tap(uint16_t keycode, noah_emit_policy_t policy) {
    noah_emit_run(keycode, tap_code16, policy);
}

void action_dispatch(uint16_t action) {
    noah_emit_action_tap(action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
}
