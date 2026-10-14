// Registered phase-3 fact: combo layer.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_COMBO_LAYER
#include "../../../compat/qmk_combo_origin.h"
void noah_lighting_slice_combo_layer_collect(uint8_t *slice) { noah_qmk_combo_origin_lighting_layers(slice); }
void noah_lighting_slice_combo_layer_idle(uint8_t *slice) { memset(slice, 0xff, NOAH_LIGHTING_COMBO_SLICE_BYTES); }
bool noah_lighting_fact_combo_layer_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    while (*cursor < COMBO_BUFFER_LENGTH) {
        const uint8_t *record = &facts->combo_layer[(*cursor)++ * NOAH_LIGHTING_COMBO_RECORD_BYTES];
        if (record[0] >= 128u || record[1] >= LAYER_COUNT || (params[0] != 255u && params[0] != record[0]) || (params[1] != 255u && params[1] != record[1])) continue;
        noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_COMBO, record[0], 0u);
        key_origin_bitmap_copy(subject->keys, record + 2u);
        subject->has_keys = true;
        subject->sides = key_origin_bitmap_side_mask(subject->keys);
        return true;
    }
    return false;
}
#endif
