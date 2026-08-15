#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "macro_payload.h"

#define MACRO_PAYLOAD_MAX_TAP_KEYS 16

static inline bool macro_payload_text_byte_is_supported(uint8_t byte) {
    return byte != 0u && byte <= 0x7Fu;
}

typedef enum {
    MACRO_PAYLOAD_COMMAND_DELAY,
    MACRO_PAYLOAD_COMMAND_KEY_DOWN,
    MACRO_PAYLOAD_COMMAND_KEY_UP,
    MACRO_PAYLOAD_COMMAND_TAP_LIST,
} macro_payload_command_kind_t;

typedef struct {
    uint8_t keycodes[MACRO_PAYLOAD_MAX_TAP_KEYS];
    uint8_t count;
} macro_payload_tap_list_t;

typedef struct {
    macro_payload_command_kind_t kind;
    uint16_t                     delay_ms;
    uint8_t                      keycode;
    macro_payload_tap_list_t     tap_list;
} macro_payload_command_t;

typedef struct {
    uint8_t keycodes[MACRO_PAYLOAD_MAX_TAP_KEYS];
    uint8_t count;
} macro_payload_hold_balance_t;

static inline void macro_payload_hold_balance_reset(macro_payload_hold_balance_t *balance) {
    if (balance) {
        balance->count = 0;
    }
}

static inline int8_t macro_payload_hold_balance_find(const macro_payload_hold_balance_t *balance, uint8_t keycode) {
    if (!balance) {
        return -1;
    }

    for (uint8_t i = 0; i < balance->count; i++) {
        if (balance->keycodes[i] == keycode) {
            return (int8_t)i;
        }
    }

    return -1;
}

static inline bool macro_payload_hold_balance_note_down(macro_payload_hold_balance_t *balance, uint8_t keycode) {
    if (!balance || balance->count >= MACRO_PAYLOAD_MAX_TAP_KEYS || macro_payload_hold_balance_find(balance, keycode) >= 0) {
        return false;
    }

    balance->keycodes[balance->count++] = keycode;
    return true;
}

static inline bool macro_payload_hold_balance_note_up(macro_payload_hold_balance_t *balance, uint8_t keycode) {
    int8_t index;

    if (!balance) {
        return false;
    }

    index = macro_payload_hold_balance_find(balance, keycode);
    if (index < 0) {
        return false;
    }

    for (uint8_t i = (uint8_t)index; i + 1u < balance->count; i++) {
        balance->keycodes[i] = balance->keycodes[i + 1u];
    }
    balance->count--;
    return true;
}

static inline bool macro_payload_hold_balance_is_clear(const macro_payload_hold_balance_t *balance) {
    return balance && balance->count == 0u;
}

bool macro_payload_lookup_keycode(const char *start, size_t length, uint8_t *keycode);
bool macro_payload_parse_command(const char *start, const char *end, macro_payload_command_t *command);
