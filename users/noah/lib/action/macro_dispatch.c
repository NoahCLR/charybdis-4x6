#include QMK_KEYBOARD_H // QMK

#include "noah_keymap.h"
#include "../macro/macro_payload.h"

bool macro_dispatch(uint16_t keycode) {
    if (keycode < MACRO_0 || keycode > MACRO_15) {
        return false;
    }

    const char *payload = hardcoded_macro_payloads[keycode - MACRO_0];
    if (payload && *payload) {
        macro_payload_play(payload);
    }

    return true;
}
