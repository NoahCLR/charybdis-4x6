// ────────────────────────────────────────────────────────────────────────────
// Keyboard Modifier Ownership
// ────────────────────────────────────────────────────────────────────────────

#include "keyboard_mod_ownership.h"

static int8_t keyboard_mod_ownership_index_for_keycode(uint16_t keycode) {
    switch (keycode) {
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

static uint8_t keyboard_mod_ownership_physical_refcounts[8] = {0};
static uint8_t keyboard_mod_ownership_owned_refcounts[8]    = {0};

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    int8_t index = keyboard_mod_ownership_index_for_keycode(keycode);

    if (index < 0 || !record || IS_NOEVENT(record->event)) {
        return;
    }

    if (record->event.pressed) {
        if (keyboard_mod_ownership_physical_refcounts[index] < UINT8_MAX) {
            keyboard_mod_ownership_physical_refcounts[index]++;
        }
    } else if (keyboard_mod_ownership_physical_refcounts[index] > 0) {
        keyboard_mod_ownership_physical_refcounts[index]--;
    }
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    int8_t index = keyboard_mod_ownership_index_for_keycode(keycode);

    if (index < 0 || !record || record->event.pressed) {
        return false;
    }

    return keyboard_mod_ownership_owned_refcounts[index] > 0;
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    int8_t index = keyboard_mod_ownership_index_for_keycode(keycode);

    if (index < 0) {
        return;
    }

    if (keyboard_mod_ownership_owned_refcounts[index]++ == 0) {
        add_mods(MOD_BIT(keycode));
        send_keyboard_report();
    }
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    int8_t index = keyboard_mod_ownership_index_for_keycode(keycode);

    if (index < 0 || keyboard_mod_ownership_owned_refcounts[index] == 0) {
        return;
    }

    keyboard_mod_ownership_owned_refcounts[index]--;
    if (keyboard_mod_ownership_owned_refcounts[index] == 0 && keyboard_mod_ownership_physical_refcounts[index] == 0) {
        del_mods(MOD_BIT(keycode));
        send_keyboard_report();
    }
}
