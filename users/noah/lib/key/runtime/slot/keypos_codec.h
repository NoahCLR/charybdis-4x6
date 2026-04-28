// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Key Position Codec
// ────────────────────────────────────────────────────────────────────────────
//
// Hot runtime effect payloads only need to preserve matrix identity. Pack
// those positions into one byte so stack-backed transition plans stay small.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t key_runtime_packed_keypos_t;

enum {
    KEY_RUNTIME_PACKED_KEYPOS_NONE = UINT8_MAX,
};

_Static_assert((MATRIX_ROWS * MATRIX_COLS) <= UINT8_MAX, "key runtime packed key positions require a matrix index that fits in one byte");

static inline bool key_runtime_keypos_is_physical(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static inline key_runtime_packed_keypos_t key_runtime_keypos_pack(keypos_t key_pos) {
    if (!key_runtime_keypos_is_physical(key_pos)) {
        return KEY_RUNTIME_PACKED_KEYPOS_NONE;
    }

    return (key_runtime_packed_keypos_t)(((uint16_t)key_pos.row * MATRIX_COLS) + key_pos.col);
}

static inline keypos_t key_runtime_keypos_unpack(key_runtime_packed_keypos_t packed_key_pos) {
    if (packed_key_pos == KEY_RUNTIME_PACKED_KEYPOS_NONE) {
        return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
    }

    return (keypos_t){
        .row = (uint8_t)(packed_key_pos / MATRIX_COLS),
        .col = (uint8_t)(packed_key_pos % MATRIX_COLS),
    };
}
