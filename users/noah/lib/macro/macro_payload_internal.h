#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "macro_payload.h"

static inline bool macro_payload_text_byte_is_supported(uint8_t byte) {
    return byte != 0u && byte <= 0x7Fu;
}

// Canonical UTF-8: no overlong encoding, surrogate or value above U+10FFFF.
// The caller supplies at most four bytes and advances only on success.
static inline bool macro_payload_utf8_scalar(const uint8_t *bytes, size_t available, uint32_t *scalar, uint8_t *width) {
    if (!bytes || !available || !scalar || !width) return false;
    uint8_t first = bytes[0];
    uint8_t count = first < 0x80u ? 1u : first >= 0xC2u && first <= 0xDFu ? 2u : first >= 0xE0u && first <= 0xEFu ? 3u : first >= 0xF0u && first <= 0xF4u ? 4u : 0u;
    if (!count || available < count) return false;
    uint32_t value = first & (count == 1u ? 0x7Fu : count == 2u ? 0x1Fu : count == 3u ? 0x0Fu : 0x07u);
    for (uint8_t i = 1u; i < count; i++) {
        if ((bytes[i] & 0xC0u) != 0x80u) return false;
        value = (value << 6u) | (bytes[i] & 0x3Fu);
    }
    if (value == 0u || (count > 1u && value < 0xA0u) || (count == 2u && value < 0x80u) || (count == 3u && value < 0x800u) || (count == 4u && value < 0x10000u) || (value >= 0xD800u && value <= 0xDFFFu) || value > 0x10FFFFu) return false;
    *scalar = value;
    *width = count;
    return true;
}

static inline bool macro_payload_ir_append_unicode(macro_payload_ir_t *ir, uint32_t scalar) {
    if (!ir || scalar < 0xA0u || scalar > 0x10FFFFu || (scalar >= 0xD800u && scalar <= 0xDFFFu) || ir->length + 4u > sizeof(ir->bytes)) return false;
    ir->bytes[ir->length++] = MACRO_PAYLOAD_IR_OP_UNICODE;
    ir->bytes[ir->length++] = (uint8_t)scalar;
    ir->bytes[ir->length++] = (uint8_t)(scalar >> 8u);
    ir->bytes[ir->length++] = (uint8_t)(scalar >> 16u);
    return true;
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
