// ────────────────────────────────────────────────────────────────────────────
// Held Action Ownership
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "held_action.h"

// A held pure modifier is owned by the physical switch that started it, not by
// whichever custom key the tap/hold FSM is currently resolving.
typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
} held_modifier_binding_t;

static held_modifier_binding_t held_modifiers[8]         = {0};
static uint8_t                 held_modifier_refcounts[8] = {0};

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
        del_mods(MOD_BIT(action));
        send_keyboard_report();
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
        add_mods(MOD_BIT(action));
        send_keyboard_report();
    }
}

bool held_modifier_release_owned_by_key(keypos_t key_pos) {
    int8_t slot = held_modifier_find_slot_for_key(key_pos);

    if (slot < 0) {
        return false;
    }

    held_modifier_remove_slot((uint8_t)slot);
    return true;
}

void held_action_register(keypos_t key_pos, uint16_t action) {
    if (held_action_is_pure_modifier(action)) {
        held_modifier_register(key_pos, action);
        return;
    }

    register_code16(action);
}

void held_action_unregister(keypos_t key_pos, uint16_t action) {
    if (held_action_is_pure_modifier(action)) {
        held_modifier_release_owned_by_key(key_pos);
        return;
    }

    unregister_code16(action);
}

bool held_action_survives_flush(uint16_t action) {
    return held_action_is_pure_modifier(action);
}
