// ────────────────────────────────────────────────────────────────────────────
// Profile-Wire v1 Semantic Action Runtime Translation
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "profile_action_runtime_v1.h"

#include "noah_keymap_ids.h"
#include "../../pointing/defs/pd_modes.h"

noah_profile_action_runtime_v1_result_t noah_profile_action_runtime_v1_from_native(uint16_t native_action, noah_profile_action_v1_t *action) {
    if (!action) {
        return NOAH_PROFILE_ACTION_RUNTIME_V1_INVALID_ARGUMENT;
    }
    *action = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_NONE};
    if (native_action == KC_NO) {
        return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
    }
    if (native_action >= MACRO_0 && native_action <= MACRO_15) {
        action->kind    = NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO;
        action->operand = (uint16_t)(native_action - MACRO_0);
        return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
    }
    if (IS_QK_MACRO(native_action)) {
        action->kind    = NOAH_PROFILE_ACTION_V1_VIA_MACRO;
        action->operand = (uint16_t)(native_action - QK_MACRO_0);
        return action->operand < VIA_MACRO_SLOT_COUNT ? NOAH_PROFILE_ACTION_RUNTIME_V1_OK : NOAH_PROFILE_ACTION_RUNTIME_V1_UNSUPPORTED;
    }
    if (native_action >= LAYER_LOCK_BASE && native_action < LAYER_LOCK_BASE + LAYER_COUNT) {
        action->kind    = NOAH_PROFILE_ACTION_V1_LAYER_LOCK;
        action->operand = (uint16_t)(native_action - LAYER_LOCK_BASE);
        return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
    }
    if (IS_QK_MOMENTARY(native_action)) {
        action->kind    = NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY;
        action->operand = QK_MOMENTARY_GET_LAYER(native_action);
        return action->operand < LAYER_COUNT ? NOAH_PROFILE_ACTION_RUNTIME_V1_OK : NOAH_PROFILE_ACTION_RUNTIME_V1_UNSUPPORTED;
    }
    for (uint8_t index = 0u; index < PD_MODE_COUNT; index++) {
        if (native_action == pd_modes[index].keycode) {
            action->kind    = NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY;
            action->operand = index;
            return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
        }
        if (native_action == pd_modes[index].lock_action) {
            action->kind    = NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK;
            action->operand = index;
            return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
        }
    }
    action->kind    = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE;
    action->operand = native_action;
    return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
}

noah_profile_action_runtime_v1_result_t noah_profile_action_runtime_v1_to_native(const noah_profile_action_v1_t *action, uint16_t *native_action) {
    if (!action || !native_action || action->flags != 0u) {
        return NOAH_PROFILE_ACTION_RUNTIME_V1_INVALID_ARGUMENT;
    }

    switch (action->kind) {
        case NOAH_PROFILE_ACTION_V1_NONE:
            *native_action = KC_NO;
            return action->operand == 0u ? NOAH_PROFILE_ACTION_RUNTIME_V1_OK : NOAH_PROFILE_ACTION_RUNTIME_V1_UNSUPPORTED;
        case NOAH_PROFILE_ACTION_V1_QMK_KEYCODE:
            *native_action = action->operand;
            return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
        case NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY:
            if (action->operand >= LAYER_COUNT) break;
            *native_action = MO(action->operand);
            return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
        case NOAH_PROFILE_ACTION_V1_LAYER_LOCK:
            if (action->operand >= LAYER_COUNT) break;
            *native_action = (uint16_t)(LAYER_LOCK_BASE + action->operand);
            return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
        case NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY:
            if (action->operand >= PD_MODE_COUNT) break;
            *native_action = pd_modes[action->operand].keycode;
            return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
        case NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK:
            if (action->operand >= PD_MODE_COUNT) break;
            *native_action = pd_modes[action->operand].lock_action;
            return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
        case NOAH_PROFILE_ACTION_V1_VIA_MACRO:
            if (action->operand >= VIA_MACRO_SLOT_COUNT) break;
            *native_action = (uint16_t)(QK_MACRO_0 + action->operand);
            return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
        case NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO:
            if (action->operand >= HARDCODED_MACRO_SLOT_COUNT) break;
            *native_action = (uint16_t)(MACRO_0 + action->operand);
            return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
        default:
            break;
    }
    *native_action = KC_NO;
    return NOAH_PROFILE_ACTION_RUNTIME_V1_UNSUPPORTED;
}
