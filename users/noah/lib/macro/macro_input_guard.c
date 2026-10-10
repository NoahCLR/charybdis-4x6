#include "macro_input_guard.h"

static uint8_t     macro_ignored[(MATRIX_ROWS * MATRIX_COLS + 7u) / 8u];

bool noah_record_admission_macro_admit(keyrecord_t *record, bool protected, bool synthetic) {
    if (!record || synthetic || record->event.type != KEY_EVENT || record->event.key.row >= MATRIX_ROWS || record->event.key.col >= MATRIX_COLS) return true;
    uint16_t position = record->event.key.row * MATRIX_COLS + record->event.key.col;
    uint8_t mask = (uint8_t)(1u << (position % 8u));
    if (macro_ignored[position / 8u] & mask) {
        if (!record->event.pressed) macro_ignored[position / 8u] &= (uint8_t)~mask;
        return false;
    }
    if (protected && record->event.pressed) {
        macro_ignored[position / 8u] |= mask;
        return false;
    }
    return true;
}

void noah_macro_input_guard_reset(void) {
    for (uint16_t index = 0u; index < sizeof(macro_ignored); index++) macro_ignored[index] = 0u;
}
