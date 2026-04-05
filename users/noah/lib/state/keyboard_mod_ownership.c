// ────────────────────────────────────────────────────────────────────────────
// Keyboard Modifier Ownership
// ────────────────────────────────────────────────────────────────────────────

#include "keyboard_mod_ownership.h"

static const uint8_t keyboard_mod_ownership_mod_masks[8] = {
    MOD_BIT(KC_LEFT_CTRL),
    MOD_BIT(KC_LEFT_SHIFT),
    MOD_BIT(KC_LEFT_ALT),
    MOD_BIT(KC_LEFT_GUI),
    MOD_BIT(KC_RIGHT_CTRL),
    MOD_BIT(KC_RIGHT_SHIFT),
    MOD_BIT(KC_RIGHT_ALT),
    MOD_BIT(KC_RIGHT_GUI),
};

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
static uint8_t keyboard_mod_ownership_managed_refcounts[8]  = {0};

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

    return keyboard_mod_ownership_managed_refcounts[index] > 0;
}

void keyboard_mod_ownership_register_mods(uint8_t mods) {
    bool report_needed = false;

    if (mods == 0) {
        return;
    }

    for (uint8_t i = 0; i < ARRAY_SIZE(keyboard_mod_ownership_mod_masks); i++) {
        if (!(mods & keyboard_mod_ownership_mod_masks[i])) {
            continue;
        }

        if (keyboard_mod_ownership_managed_refcounts[i] < UINT8_MAX && keyboard_mod_ownership_managed_refcounts[i]++ == 0) {
            add_mods(keyboard_mod_ownership_mod_masks[i]);
            report_needed = true;
        }
    }

    if (report_needed) {
        send_keyboard_report();
    }
}

void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    bool report_needed = false;

    if (mods == 0) {
        return;
    }

    for (uint8_t i = 0; i < ARRAY_SIZE(keyboard_mod_ownership_mod_masks); i++) {
        if (!(mods & keyboard_mod_ownership_mod_masks[i]) || keyboard_mod_ownership_managed_refcounts[i] == 0) {
            continue;
        }

        keyboard_mod_ownership_managed_refcounts[i]--;
        if (keyboard_mod_ownership_managed_refcounts[i] == 0 && keyboard_mod_ownership_physical_refcounts[i] == 0) {
            del_mods(keyboard_mod_ownership_mod_masks[i]);
            report_needed = true;
        }
    }

    if (report_needed) {
        send_keyboard_report();
    }
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    int8_t index = keyboard_mod_ownership_index_for_keycode(keycode);

    if (index < 0) {
        return;
    }

    keyboard_mod_ownership_register_mods(keyboard_mod_ownership_mod_masks[index]);
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    int8_t index = keyboard_mod_ownership_index_for_keycode(keycode);

    if (index < 0) {
        return;
    }

    keyboard_mod_ownership_unregister_mods(keyboard_mod_ownership_mod_masks[index]);
}

void register_mods(uint8_t mods) {
    keyboard_mod_ownership_register_mods(mods);
}

void unregister_mods(uint8_t mods) {
    keyboard_mod_ownership_unregister_mods(mods);
}
