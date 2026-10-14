// Registered phase-3 fact: key pressed.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_KEY_PRESSED
#include "matrix.h"
#include "fact_key_subject.h"
void noah_lighting_slice_key_pressed_collect(uint8_t *slice) {
    key_origin_bitmap_clear(slice);
    for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
        matrix_row_t keys = matrix_get_row(row);
        for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
            if (keys & ((matrix_row_t)1u << col)) key_origin_bitmap_add_keypos(slice, (keypos_t){.row = row, .col = col});
        }
    }
}
void noah_lighting_slice_key_pressed_idle(uint8_t *slice) { key_origin_bitmap_clear(slice); }
bool noah_lighting_fact_key_pressed_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    while (*cursor < MATRIX_ROWS * MATRIX_COLS) {
        uint8_t key = (*cursor)++;
        if ((params[0] != 255u && params[0] != key) || !key_origin_bitmap_has_keypos(facts->key_pressed, (keypos_t){.row = key / MATRIX_COLS, .col = key % MATRIX_COLS})) continue;
        noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_KEY, key, 0u);
        noah_lighting_subject_key(subject, key);
        return true;
    }
    return false;
}
#endif
