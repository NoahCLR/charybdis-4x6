// Fact 7: identified held combos, newest first. Fact 4 stays aggregate.
#include "../lighting.h"
#include "../../../compat/qmk_combo_origin.h"
#include "../../../key/runtime/reducer/state_query.h"
#include "../../../state/ownership/layer_ownership.h"
#include "../../../pointing/defs/pd_modes.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_COMBO_HELD
void noah_lighting_slice_combo_held_idle(uint8_t *slice) {
    memset(slice, 0xff, NOAH_LIGHTING_COMBO_SLICE_BYTES);
}
void noah_lighting_slice_combo_held_collect(uint8_t *slice) {
    keypos_t preview = {.row = MATRIX_ROWS, .col = MATRIX_COLS};
    keypos_t mode    = preview;
    uint8_t  quiet[KEY_ORIGIN_BITMAP_SIZE];
    uint16_t cursor = 0u;
    noah_lighting_slice_combo_held_idle(slice);
    (void)key_runtime_core_preview_owner_key_pos(&preview);
#    ifdef POINTING_DEVICE_ENABLE
    (void)pd_mode_local_owner_key_pos_snapshot(&mode);
#    endif
    layer_ownership_momentary_owner_bitmap(quiet);
    for (uint8_t i = 0; i < COMBO_BUFFER_LENGTH; i++) {
        uint8_t *record = &slice[i * NOAH_LIGHTING_COMBO_RECORD_BYTES];
        if (!noah_qmk_combo_origin_lighting_next(preview, mode, quiet, &cursor, record, record + 1u, record + 2u)) break;
    }
}
bool noah_lighting_fact_combo_held_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    while (*cursor < COMBO_BUFFER_LENGTH) {
        const uint8_t *record = &facts->combo_held[(*cursor)++ * NOAH_LIGHTING_COMBO_RECORD_BYTES];
        if (record[0] >= 128u || (params[0] != 255u && params[0] != record[0]) || (params[1] != 255u && params[1] != record[1])) continue;
        noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_COMBO, record[0], 0u);
        key_origin_bitmap_copy(subject->keys, record + 2u);
        subject->has_keys = true;
        subject->sides    = key_origin_bitmap_side_mask(subject->keys);
        return true;
    }
    return false;
}
#endif
