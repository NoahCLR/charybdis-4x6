// ────────────────────────────────────────────────────────────────────────────
// Action Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "noah_keymap.h"
#include "action_lifecycle.h"
#include "../key/key_runtime_effects.h"
#include "../state/layer_ownership.h"
#include "action_dispatch.h"

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

void action_dispatch(uint16_t action) {
    key_runtime_effects_activate_pending_fallback_hold();
    noah_action_tap(action);
}
