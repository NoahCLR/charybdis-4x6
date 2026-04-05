// ────────────────────────────────────────────────────────────────────────────
// Held Action Ownership
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "noah_keymap.h"
#include "../action/action_dispatch.h"
#include "../pointing/pointing_device_modes.h"
#include "../state/keyboard_mod_ownership.h"
#include "../state/layer_ownership.h"
#include "held_action.h"
#include "../action/synthetic_record.h"

// A held pure modifier is owned by the physical switch that started it, not by
// whichever custom key the tap/hold FSM is currently resolving.
typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
} held_modifier_binding_t;

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
} held_action_binding_t;

static held_modifier_binding_t held_modifiers[8]          = {0};
static uint8_t                 held_modifier_refcounts[8] = {0};
static held_action_binding_t   held_actions[8]            = {0};

static inline bool keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static bool held_action_is_pure_modifier(uint16_t action) {
    switch (action) {
        case KC_LEFT_CTRL:
        case KC_LEFT_SHIFT:
        case KC_LEFT_ALT:
        case KC_LEFT_GUI:
        case KC_RIGHT_CTRL:
        case KC_RIGHT_SHIFT:
        case KC_RIGHT_ALT:
        case KC_RIGHT_GUI:
            return true;
        default:
            return false;
    }
}

static bool held_action_is_owned_momentary_layer(uint16_t action) {
    return IS_QK_MOMENTARY(action);
}

static bool held_action_is_owned_modded_keycode(uint16_t action) {
    return IS_QK_MODS(action);
}

static uint8_t held_action_extract_mods(uint16_t action) {
    uint8_t mods_to_send = 0;

    if (action & QK_RMODS_MIN) {
        if (action & QK_LCTL) mods_to_send |= MOD_BIT(KC_RIGHT_CTRL);
        if (action & QK_LSFT) mods_to_send |= MOD_BIT(KC_RIGHT_SHIFT);
        if (action & QK_LALT) mods_to_send |= MOD_BIT(KC_RIGHT_ALT);
        if (action & QK_LGUI) mods_to_send |= MOD_BIT(KC_RIGHT_GUI);
    } else {
        if (action & QK_LCTL) mods_to_send |= MOD_BIT(KC_LEFT_CTRL);
        if (action & QK_LSFT) mods_to_send |= MOD_BIT(KC_LEFT_SHIFT);
        if (action & QK_LALT) mods_to_send |= MOD_BIT(KC_LEFT_ALT);
        if (action & QK_LGUI) mods_to_send |= MOD_BIT(KC_LEFT_GUI);
    }

    return mods_to_send;
}

static void held_action_register_owned_modded_keycode(uint16_t action) {
    uint8_t mods    = held_action_extract_mods(action);
    uint8_t keycode = QK_MODS_GET_BASIC_KEYCODE(action);

    keyboard_mod_ownership_register_mods(mods);
    if (keycode != KC_NO) {
        register_code(keycode);
    }
}

static void held_action_unregister_owned_modded_keycode(uint16_t action) {
    uint8_t mods    = held_action_extract_mods(action);
    uint8_t keycode = QK_MODS_GET_BASIC_KEYCODE(action);

    if (keycode != KC_NO) {
        unregister_code(keycode);
    }
    keyboard_mod_ownership_unregister_mods(mods);
}

static bool held_action_requires_per_key_dispatch(uint16_t action) {
    return held_action_is_owned_momentary_layer(action);
}

static void held_action_log_unsupported_layer_action(uint16_t action) {
#ifdef CONSOLE_ENABLE
    uprintf("Unsupported held raw QMK layer action 0x%04X; use PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) for owned layer holds\n", (unsigned int)action);
#else
    (void)action;
#endif
}

static int8_t held_modifier_index_for_action(uint16_t action) {
    switch (action) {
        case KC_LEFT_CTRL:
            return 0;
        case KC_LEFT_SHIFT:
            return 1;
        case KC_LEFT_ALT:
            return 2;
        case KC_LEFT_GUI:
            return 3;
        case KC_RIGHT_CTRL:
            return 4;
        case KC_RIGHT_SHIFT:
            return 5;
        case KC_RIGHT_ALT:
            return 6;
        case KC_RIGHT_GUI:
            return 7;
        default:
            return -1;
    }
}

static int8_t held_modifier_find_slot_for_key(keypos_t key_pos) {
    for (uint8_t i = 0; i < ARRAY_SIZE(held_modifiers); i++) {
        if (held_modifiers[i].active && keypos_equal(held_modifiers[i].key_pos, key_pos)) return (int8_t)i;
    }

    return -1;
}

static int8_t held_modifier_find_free_slot(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(held_modifiers); i++) {
        if (!held_modifiers[i].active) return (int8_t)i;
    }

    return -1;
}

static int8_t held_action_find_slot_for_key(keypos_t key_pos) {
    for (uint8_t i = 0; i < ARRAY_SIZE(held_actions); i++) {
        if (held_actions[i].active && keypos_equal(held_actions[i].key_pos, key_pos)) return (int8_t)i;
    }

    return -1;
}

static int8_t held_action_find_free_slot(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(held_actions); i++) {
        if (!held_actions[i].active) return (int8_t)i;
    }

    return -1;
}

static uint8_t held_action_refcount(uint16_t action) {
    uint8_t count = 0;

    for (uint8_t i = 0; i < ARRAY_SIZE(held_actions); i++) {
        if (held_actions[i].active && held_actions[i].action == action) {
            count++;
        }
    }

    return count;
}

static void held_modifier_remove_slot(uint8_t slot) {
    uint16_t action = held_modifiers[slot].action;
    int8_t   index  = held_modifier_index_for_action(action);

    held_modifiers[slot].active = false;
    held_modifiers[slot].action = KC_NO;

    if (index < 0 || held_modifier_refcounts[index] == 0) {
        return;
    }

    held_modifier_refcounts[index]--;
    if (held_modifier_refcounts[index] == 0) {
        keyboard_mod_ownership_unregister(action);
    }
}

static void held_modifier_register(keypos_t key_pos, uint16_t action) {
    int8_t slot  = held_modifier_find_slot_for_key(key_pos);
    int8_t index = held_modifier_index_for_action(action);

    if (index < 0) {
        return;
    }

    if (slot >= 0) {
        if (held_modifiers[slot].action == action) {
            return;
        }
        held_modifier_remove_slot((uint8_t)slot);
    } else {
        slot = held_modifier_find_free_slot();
        if (slot < 0) {
            return;
        }
    }

    held_modifiers[slot] = (held_modifier_binding_t){
        .active  = true,
        .key_pos = key_pos,
        .action  = action,
    };

    if (held_modifier_refcounts[index]++ == 0) {
        keyboard_mod_ownership_register(action);
    }
}

static void held_action_dispatch_press(keypos_t key_pos, uint16_t action) {
    if (pd_mode_handle_keycode_press(action)) {
        return;
    }

    if (held_action_is_owned_momentary_layer(action)) {
        layer_ownership_momentary_press(key_pos, QK_MOMENTARY_GET_LAYER(action));
        return;
    }

    if (action_dispatch_is_raw_qmk_layer_action(action)) {
        held_action_log_unsupported_layer_action(action);
        return;
    }

    if (action_dispatch_is_qmk_behavior_keycode(action)) {
        noah_dispatch_synthetic_qmk_record(action, true, 0);
        return;
    }

    if (action >= NOAH_KEYMAP_SAFE_RANGE) {
        noah_dispatch_synthetic_record(action, true);
        return;
    }

    // Held QK_MODS keycodes, e.g. S(KC_1) or G(KC_C), must use owned real mods
    // plus the underlying base key. Leaving them on register_code16() would keep
    // the weak-mod overlap bug we are explicitly trying to avoid on held actions.
    if (held_action_is_owned_modded_keycode(action)) {
        held_action_register_owned_modded_keycode(action);
        return;
    }

    register_code16(action);
}

static void held_action_dispatch_release(keypos_t key_pos, uint16_t action) {
    if (pd_mode_handle_keycode_release(action)) {
        return;
    }

    if (held_action_is_owned_momentary_layer(action)) {
        layer_ownership_momentary_release(key_pos);
        return;
    }

    if (action_dispatch_is_raw_qmk_layer_action(action)) {
        held_action_log_unsupported_layer_action(action);
        return;
    }

    if (action_dispatch_is_qmk_behavior_keycode(action)) {
        noah_dispatch_synthetic_qmk_record(action, false, 0);
        return;
    }

    if (action >= NOAH_KEYMAP_SAFE_RANGE) {
        noah_dispatch_synthetic_record(action, false);
        return;
    }

    if (held_action_is_owned_modded_keycode(action)) {
        held_action_unregister_owned_modded_keycode(action);
        return;
    }

    unregister_code16(action);
}

static bool held_action_register_owned(keypos_t key_pos, uint16_t action) {
    int8_t slot = held_action_find_slot_for_key(key_pos);

    if (slot >= 0) {
        if (held_actions[slot].action == action) {
            return true;
        }

        uint16_t old_action       = held_actions[slot].action;
        held_actions[slot].active = false;
        held_actions[slot].action = KC_NO;
        if (held_action_refcount(old_action) == 0 || held_action_requires_per_key_dispatch(old_action)) {
            held_action_dispatch_release(key_pos, old_action);
        }
    } else {
        slot = held_action_find_free_slot();
        if (slot < 0) {
            return false;
        }
    }

    bool first_binding = held_action_refcount(action) == 0;
    held_actions[slot] = (held_action_binding_t){
        .active  = true,
        .key_pos = key_pos,
        .action  = action,
    };

    if (first_binding || held_action_requires_per_key_dispatch(action)) {
        held_action_dispatch_press(key_pos, action);
    }

    return true;
}

bool held_modifier_release_owned_by_key(keypos_t key_pos) {
    int8_t slot = held_modifier_find_slot_for_key(key_pos);

    if (slot < 0) {
        return false;
    }

    held_modifier_remove_slot((uint8_t)slot);
    return true;
}

bool held_action_release_owned_by_key(keypos_t key_pos) {
    int8_t slot = held_action_find_slot_for_key(key_pos);

    if (slot >= 0) {
        uint16_t action           = held_actions[slot].action;
        held_actions[slot].active = false;
        held_actions[slot].action = KC_NO;

        if (held_action_refcount(action) == 0 || held_action_requires_per_key_dispatch(action)) {
            held_action_dispatch_release(key_pos, action);
        }
        return true;
    }

    return held_modifier_release_owned_by_key(key_pos);
}

void held_action_register(keypos_t key_pos, uint16_t action) {
    if (held_action_is_pure_modifier(action)) {
        held_modifier_register(key_pos, action);
        return;
    }

    if (held_action_register_owned(key_pos, action)) {
        return;
    }

    held_action_dispatch_press(key_pos, action);
}

void held_action_unregister(keypos_t key_pos, uint16_t action) {
    if (held_action_is_pure_modifier(action)) {
        held_modifier_release_owned_by_key(key_pos);
        return;
    }

    if (held_action_release_owned_by_key(key_pos)) {
        return;
    }

    held_action_dispatch_release(key_pos, action);
}

bool held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    if (held_action_is_pure_modifier(action)) {
        return true;
    }

    int8_t slot = held_action_find_slot_for_key(key_pos);
    return slot >= 0 && held_actions[slot].action == action;
}
