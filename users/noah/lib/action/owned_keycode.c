// ────────────────────────────────────────────────────────────────────────────
// Owned Keycode Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include "owned_keycode.h"

#include "../pointing/policy/pointer_layer_policy.h"
#include "../state/ownership/keyboard_mod_ownership.h"

static bool owned_keycode_is_modded(uint16_t keycode) {
    return IS_QK_MODS(keycode);
}

static uint8_t owned_keycode_extract_mods(uint16_t keycode) {
    uint8_t mods_to_send = 0;

    if (keycode & QK_RMODS_MIN) {
        if (keycode & QK_LCTL) mods_to_send |= MOD_BIT(KC_RIGHT_CTRL);
        if (keycode & QK_LSFT) mods_to_send |= MOD_BIT(KC_RIGHT_SHIFT);
        if (keycode & QK_LALT) mods_to_send |= MOD_BIT(KC_RIGHT_ALT);
        if (keycode & QK_LGUI) mods_to_send |= MOD_BIT(KC_RIGHT_GUI);
    } else {
        if (keycode & QK_LCTL) mods_to_send |= MOD_BIT(KC_LEFT_CTRL);
        if (keycode & QK_LSFT) mods_to_send |= MOD_BIT(KC_LEFT_SHIFT);
        if (keycode & QK_LALT) mods_to_send |= MOD_BIT(KC_LEFT_ALT);
        if (keycode & QK_LGUI) mods_to_send |= MOD_BIT(KC_LEFT_GUI);
    }

    return mods_to_send;
}

static uint8_t owned_keycode_basic_part(uint16_t keycode) {
    return owned_keycode_is_modded(keycode) ? QK_MODS_GET_BASIC_KEYCODE(keycode) : (uint8_t)keycode;
}

bool owned_keycode_register(uint16_t keycode) {
    if (owned_keycode_is_modded(keycode)) {
        uint8_t basic = owned_keycode_basic_part(keycode);

        keyboard_mod_ownership_register_mods(owned_keycode_extract_mods(keycode));
        if (basic != KC_NO) {
            pointer_layer_policy_note_action(basic, true);
            register_code(basic);
        }
        return true;
    }

    if (keycode > UINT8_MAX) {
        return false;
    }

    if (IS_MODIFIER_KEYCODE(keycode)) {
        keyboard_mod_ownership_register(keycode);
    } else {
        pointer_layer_policy_note_action(keycode, true);
        register_code((uint8_t)keycode);
    }

    return true;
}

bool owned_keycode_unregister(uint16_t keycode) {
    if (owned_keycode_is_modded(keycode)) {
        uint8_t basic = owned_keycode_basic_part(keycode);

        if (basic != KC_NO) {
            unregister_code(basic);
            pointer_layer_policy_note_action(basic, false);
        }
        keyboard_mod_ownership_unregister_mods(owned_keycode_extract_mods(keycode));
        return true;
    }

    if (keycode > UINT8_MAX) {
        return false;
    }

    if (IS_MODIFIER_KEYCODE(keycode)) {
        keyboard_mod_ownership_unregister(keycode);
    } else {
        unregister_code((uint8_t)keycode);
        pointer_layer_policy_note_action(keycode, false);
    }

    return true;
}

bool owned_keycode_tap(uint16_t keycode) {
    if (!owned_keycode_register(keycode)) {
        return false;
    }

    uint8_t  basic = owned_keycode_basic_part(keycode);
    uint16_t delay = (basic == KC_CAPS_LOCK) ? TAP_HOLD_CAPS_DELAY : TAP_CODE_DELAY;

    wait_ms(delay);
    (void)owned_keycode_unregister(keycode);
    return true;
}
