#include QMK_KEYBOARD_H // IWYU pragma: keep

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "noah_keymap.h"
#include "../macro/macro_payload.h"

typedef enum {
    MACRO_DISPATCH_SLOT_UNCHECKED = 0,
    MACRO_DISPATCH_SLOT_VALID,
    MACRO_DISPATCH_SLOT_INVALID,
} macro_dispatch_slot_state_t;

static uint8_t hardcoded_macro_slot_state[HARDCODED_MACRO_SLOT_COUNT];

static void macro_dispatch_log_invalid_payload(uint8_t slot, const char *payload) {
#ifdef CONSOLE_ENABLE
    uprintf("Invalid hardcoded macro payload for MACRO_%u: %s\n", (unsigned int)slot, payload);
#else
    (void)slot;
    (void)payload;
#endif
}

static bool macro_dispatch_slot_is_valid(uint8_t slot) {
    const char *payload = hardcoded_macro_payloads[slot];

    if (hardcoded_macro_slot_state[slot] == MACRO_DISPATCH_SLOT_VALID) {
        return true;
    }
    if (hardcoded_macro_slot_state[slot] == MACRO_DISPATCH_SLOT_INVALID) {
        return false;
    }

    if (!payload || !*payload || macro_payload_validate(payload)) {
        hardcoded_macro_slot_state[slot] = MACRO_DISPATCH_SLOT_VALID;
        return true;
    }

    hardcoded_macro_slot_state[slot] = MACRO_DISPATCH_SLOT_INVALID;
    macro_dispatch_log_invalid_payload(slot, payload);
    return false;
}

void macro_dispatch_validate_all(void) {
    for (uint8_t slot = 0; slot < HARDCODED_MACRO_SLOT_COUNT; slot++) {
        (void)macro_dispatch_slot_is_valid(slot);
    }
}

bool macro_dispatch(uint16_t keycode) {
    uint8_t slot = 0;
    if (keycode < MACRO_0 || keycode > MACRO_15) {
        return false;
    }

    slot = (uint8_t)(keycode - MACRO_0);

    const char *payload = hardcoded_macro_payloads[slot];
    if (payload && *payload && macro_dispatch_slot_is_valid(slot) && !macro_payload_play(payload)) {
        hardcoded_macro_slot_state[slot] = MACRO_DISPATCH_SLOT_INVALID;
        macro_dispatch_log_invalid_payload(slot, payload);
    }

    return true;
}
