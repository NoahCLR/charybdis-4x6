// ────────────────────────────────────────────────────────────────────────────
// Key Origin Registry
// ────────────────────────────────────────────────────────────────────────────

#include "origin_registry.h"

static uint8_t key_origin_registry_data[MATRIX_ROWS * MATRIX_COLS][KEY_ORIGIN_BITMAP_SIZE];

void key_origin_registry_reset(void) {
    for (uint16_t index = 0; index < ARRAY_SIZE(key_origin_registry_data); index++) {
        key_origin_bitmap_clear(key_origin_registry_data[index]);
    }
}

void key_origin_registry_init(void) {
    key_origin_registry_reset();
}

void key_origin_registry_set_single(keypos_t owner_key_pos) {
    if (!key_origin_keypos_valid(owner_key_pos)) {
        return;
    }

    key_origin_bitmap_fill_single(key_origin_registry_data[key_origin_keypos_index(owner_key_pos)], owner_key_pos);
}

bool key_origin_registry_set_bitmap(keypos_t owner_key_pos, const uint8_t *bitmap) {
    uint8_t *entry;

    if (!key_origin_keypos_valid(owner_key_pos)) {
        return false;
    }

    entry = key_origin_registry_data[key_origin_keypos_index(owner_key_pos)];
    if (bitmap && key_origin_bitmap_has_any(bitmap)) {
        key_origin_bitmap_copy(entry, bitmap);
    } else {
        key_origin_bitmap_clear(entry);
    }

    return true;
}

bool key_origin_registry_get_bitmap(keypos_t owner_key_pos, uint8_t *out_bitmap) {
    const uint8_t *entry;

    if (!out_bitmap) {
        return false;
    }

    key_origin_bitmap_clear(out_bitmap);
    if (!key_origin_keypos_valid(owner_key_pos)) {
        return false;
    }

    entry = key_origin_registry_data[key_origin_keypos_index(owner_key_pos)];
    if (key_origin_bitmap_has_any(entry)) {
        key_origin_bitmap_copy(out_bitmap, entry);
    } else {
        key_origin_bitmap_fill_single(out_bitmap, owner_key_pos);
    }

    return true;
}

split_side_mask_t key_origin_registry_side_mask(keypos_t owner_key_pos) {
    uint8_t bitmap[KEY_ORIGIN_BITMAP_SIZE];

    if (!key_origin_registry_get_bitmap(owner_key_pos, bitmap)) {
        return SPLIT_SIDE_MASK_NONE;
    }

    return key_origin_bitmap_side_mask(bitmap);
}
