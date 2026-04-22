// ────────────────────────────────────────────────────────────────────────────
// Split Half Helpers
// ────────────────────────────────────────────────────────────────────────────
//
// Shared split-keyboard half vocabulary plus helpers for resolving a physical
// matrix position to a left/right half.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdint.h>

typedef uint8_t split_half_t;

enum {
    SPLIT_HALF_NONE = 0,
    SPLIT_HALF_LEFT,
    SPLIT_HALF_RIGHT,
};

static inline split_half_t split_half_from_keypos(keypos_t key_pos) {
    if (key_pos.row >= MATRIX_ROWS || key_pos.col >= MATRIX_COLS) {
        return SPLIT_HALF_NONE;
    }

    return key_pos.row < (MATRIX_ROWS / 2u) ? SPLIT_HALF_LEFT : SPLIT_HALF_RIGHT;
}
