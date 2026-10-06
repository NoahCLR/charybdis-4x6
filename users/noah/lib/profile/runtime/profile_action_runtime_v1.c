// ────────────────────────────────────────────────────────────────────────────
// Profile-Wire v1 Semantic Action Runtime Translation
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "profile_action_runtime_v1.h"

#include "noah_keymap_ids.h"
#include "../../pointing/defs/pd_modes.h"

_Static_assert(CUSTOM_KEY_SLOT_COUNT == NOAH_PROFILE_ACTION_V1_MAX_CUSTOM_KEYS, "custom key identities drifted from the action ABI");
_Static_assert(PD_SLOT_31 == PD_SLOT_0 + PD_MODE_COUNT - 1 && PD_SLOT_31_LOCK == PD_SLOT_0_LOCK + PD_MODE_COUNT - 1, "pointing slot keycodes are their block's first plus the slot ID");

noah_profile_action_runtime_v1_result_t noah_profile_action_runtime_v1_from_native(uint16_t native_action, noah_profile_action_v1_t *action) {
    if (!action) {
        return NOAH_PROFILE_ACTION_RUNTIME_V1_INVALID_ARGUMENT;
    }
    *action = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_NONE};
    if (native_action == KC_NO) {
        return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
    }
    if (NOAH_KEYCODE_IS_CUSTOM_KEY(native_action)) {
        action->kind    = NOAH_PROFILE_ACTION_V1_CUSTOM_KEY;
        action->operand = (uint16_t)(native_action - CUSTOM_KEY_0);
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
    // The hold and lock blocks hold the slots in ID order, so a slot's keycode
    // is its block's first plus its ID. Behaviour canonicalization converts
    // every action many times over; scanning 32 slots for each one starved
    // the restart watchdog while a host read the compiled profile.
    if (native_action >= PD_SLOT_0 && native_action < PD_SLOT_0 + PD_MODE_COUNT) {
        action->kind    = NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY;
        action->operand = (uint16_t)(native_action - PD_SLOT_0);
        return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
    }
    if (native_action >= PD_SLOT_0_LOCK && native_action < PD_SLOT_0_LOCK + PD_MODE_COUNT) {
        action->kind    = NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK;
        action->operand = (uint16_t)(native_action - PD_SLOT_0_LOCK);
        return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
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
        case NOAH_PROFILE_ACTION_V1_CUSTOM_KEY:
            if (action->operand >= NOAH_PROFILE_ACTION_V1_MAX_CUSTOM_KEYS) break;
            *native_action = (uint16_t)(CUSTOM_KEY_0 + action->operand);
            return NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
        default:
            break;
    }
    *native_action = KC_NO;
    return NOAH_PROFILE_ACTION_RUNTIME_V1_UNSUPPORTED;
}
