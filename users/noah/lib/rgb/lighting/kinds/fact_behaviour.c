// Fact 8: one identified behaviour instance per key, in matrix order.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_BEHAVIOUR
void noah_lighting_slice_behaviour_idle(uint8_t *slice) {
    memset(slice, 0, KEY_FEEDBACK_LIGHTING_BYTES);
}
void noah_lighting_slice_behaviour_collect(uint8_t *slice) {
    key_feedback_lighting_map(slice);
}
bool noah_lighting_fact_behaviour_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    while (*cursor < MATRIX_ROWS * MATRIX_COLS) {
        uint16_t       key    = (*cursor)++;
        const uint8_t *record = &facts->behaviour[key * KEY_FEEDBACK_LIGHTING_RECORD_BYTES];
        if (record[0] == 0u || record[0] > 128u || record[1] == 0u) continue;
        uint8_t id = record[0] - 1u, slot = record[1] - 1u;
        uint8_t state = slot == 0u ? NOAH_LIGHTING_STATE_HOLD : slot == 1u ? NOAH_LIGHTING_STATE_LONG_HOLD : slot == 2u ? NOAH_LIGHTING_STATE_TAP : NOAH_LIGHTING_STATE_BRANCH;
        if ((params[0] != 255u && params[0] != id) || !(params[1] & state)) continue;
        noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_BEHAVIOUR, id, slot);
        subject->outcome = record[2];
        key_origin_bitmap_fill_single(subject->keys, (keypos_t){.row = key / MATRIX_COLS, .col = key % MATRIX_COLS});
        subject->has_keys = true;
        subject->sides    = key_origin_bitmap_side_mask(subject->keys);
        return true;
    }
    return false;
}
#endif
