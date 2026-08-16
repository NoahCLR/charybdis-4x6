// ────────────────────────────────────────────────────────────────────────────
// Split Half Helpers
// ────────────────────────────────────────────────────────────────────────────
//
// Shared split-keyboard half vocabulary plus helpers for resolving a physical
// matrix position to a left/right half or a left/right/both side mask.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdint.h>

typedef uint8_t split_half_t;
typedef uint8_t split_side_mask_t;

enum {
    SPLIT_HALF_NONE = 0,
    SPLIT_HALF_LEFT,
    SPLIT_HALF_RIGHT,
};

enum {
    SPLIT_SIDE_MASK_NONE  = 0,
    SPLIT_SIDE_MASK_LEFT  = (1u << 0),
    SPLIT_SIDE_MASK_RIGHT = (1u << 1),
    SPLIT_SIDE_MASK_BOTH  = SPLIT_SIDE_MASK_LEFT | SPLIT_SIDE_MASK_RIGHT,
};

static inline split_half_t split_half_from_keypos(keypos_t key_pos) {
    if (key_pos.row >= MATRIX_ROWS || key_pos.col >= MATRIX_COLS) {
        return SPLIT_HALF_NONE;
    }

    return key_pos.row < (MATRIX_ROWS / 2u) ? SPLIT_HALF_LEFT : SPLIT_HALF_RIGHT;
}

static inline split_side_mask_t split_side_mask_from_half(split_half_t half) {
    switch (half) {
        case SPLIT_HALF_LEFT:
            return SPLIT_SIDE_MASK_LEFT;
        case SPLIT_HALF_RIGHT:
            return SPLIT_SIDE_MASK_RIGHT;
        default:
            return SPLIT_SIDE_MASK_NONE;
    }
}

static inline split_side_mask_t split_side_mask_add_half(split_side_mask_t sides, split_half_t half) {
    return (split_side_mask_t)(sides | split_side_mask_from_half(half));
}

static inline split_side_mask_t split_side_mask_add_keypos(split_side_mask_t sides, keypos_t key_pos) {
    return split_side_mask_add_half(sides, split_half_from_keypos(key_pos));
}

