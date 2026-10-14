#pragma once
#include "../lighting.h"
static inline void noah_lighting_subject_key(noah_lighting_subject_t *subject, uint8_t key) {
    key_origin_bitmap_fill_single(subject->keys, (keypos_t){.row = key / MATRIX_COLS, .col = key % MATRIX_COLS});
    subject->has_keys = true;
    subject->sides = key_origin_bitmap_side_mask(subject->keys);
}
