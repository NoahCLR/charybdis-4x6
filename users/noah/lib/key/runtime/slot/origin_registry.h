// ────────────────────────────────────────────────────────────────────────────
// Key Origin Registry
// ────────────────────────────────────────────────────────────────────────────
//
// Tracks the RGB / PD locality footprint associated with a representative
// owner key. Physical keys default to a one-key footprint. Combo outputs can
// widen that owner key to a multi-key footprint without changing the runtime's
// single-key ownership model.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "../../../compat/split_half.h"

#define KEY_ORIGIN_BITMAP_SIZE (((MATRIX_ROWS * MATRIX_COLS) + 7u) / 8u)

static inline bool key_origin_keypos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static inline uint16_t key_origin_keypos_index(keypos_t key_pos) {
    return (uint16_t)((uint16_t)key_pos.row * MATRIX_COLS + key_pos.col);
}

static inline void key_origin_bitmap_clear(uint8_t *bitmap) {
    if (!bitmap) {
        return;
    }

    for (uint8_t index = 0; index < KEY_ORIGIN_BITMAP_SIZE; index++) {
        bitmap[index] = 0u;
    }
}

static inline void key_origin_bitmap_copy(uint8_t *dest, const uint8_t *src) {
    if (!(dest && src)) {
        return;
    }

    for (uint8_t index = 0; index < KEY_ORIGIN_BITMAP_SIZE; index++) {
        dest[index] = src[index];
    }
}

static inline void key_origin_bitmap_or_inplace(uint8_t *dest, const uint8_t *src) {
    if (!(dest && src)) {
        return;
    }

    for (uint8_t index = 0; index < KEY_ORIGIN_BITMAP_SIZE; index++) {
        dest[index] |= src[index];
    }
}

static inline void key_origin_bitmap_add_keypos(uint8_t *bitmap, keypos_t key_pos) {
    uint16_t key_index;

    if (!(bitmap && key_origin_keypos_valid(key_pos))) {
        return;
    }

    key_index = key_origin_keypos_index(key_pos);
    bitmap[key_index / 8u] |= (uint8_t)(1u << (key_index % 8u));
}

static inline void key_origin_bitmap_remove_keypos(uint8_t *bitmap, keypos_t key_pos) {
    uint16_t key_index;

    if (!(bitmap && key_origin_keypos_valid(key_pos))) {
        return;
    }

    key_index = key_origin_keypos_index(key_pos);
    bitmap[key_index / 8u] &= (uint8_t)~(1u << (key_index % 8u));
}

static inline bool key_origin_bitmap_has_keypos(const uint8_t *bitmap, keypos_t key_pos) {
    uint16_t key_index;

    if (!(bitmap && key_origin_keypos_valid(key_pos))) {
        return false;
    }

    key_index = key_origin_keypos_index(key_pos);
    return (bitmap[key_index / 8u] & (uint8_t)(1u << (key_index % 8u))) != 0u;
}

static inline void key_origin_bitmap_fill_single(uint8_t *bitmap, keypos_t key_pos) {
    key_origin_bitmap_clear(bitmap);
    key_origin_bitmap_add_keypos(bitmap, key_pos);
}

static inline bool key_origin_bitmap_has_any(const uint8_t *bitmap) {
    if (!bitmap) {
        return false;
    }

    for (uint8_t index = 0; index < KEY_ORIGIN_BITMAP_SIZE; index++) {
        if (bitmap[index] != 0u) {
            return true;
        }
    }

    return false;
}

static inline split_side_mask_t key_origin_bitmap_side_mask(const uint8_t *bitmap) {
    split_side_mask_t sides = SPLIT_SIDE_MASK_NONE;

    if (!bitmap) {
        return SPLIT_SIDE_MASK_NONE;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {.row = row, .col = col};

            if (key_origin_bitmap_has_keypos(bitmap, key_pos)) {
                sides = split_side_mask_add_keypos(sides, key_pos);
            }
        }
    }

    return sides;
}

void              key_origin_registry_init(void);
void              key_origin_registry_reset(void);
void              key_origin_registry_set_single(keypos_t owner_key_pos);
bool              key_origin_registry_set_bitmap(keypos_t owner_key_pos, const uint8_t *bitmap);
bool              key_origin_registry_get_bitmap(keypos_t owner_key_pos, uint8_t *out_bitmap);
split_side_mask_t key_origin_registry_side_mask(keypos_t owner_key_pos);
