// Registered phase-3 fact: layer status.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_LAYER_STATUS
#include "../../../state/ownership/layer_ownership.h"
#include "../../../compat/qmk_auto_mouse_contract.h"
_Static_assert(NOAH_LIGHTING_LAYER_LOCK_OFFSET == LAYER_OWNERSHIP_LIGHTING_LOCK_OFFSET, "layer lock layout");
_Static_assert(NOAH_LIGHTING_LAYER_ONESHOT_OFFSET == LAYER_OWNERSHIP_LIGHTING_ONESHOT_OFFSET, "one-shot layout");
// Owners are layer+1 per matrix key. Locked/one-shot/top retain their own truth.
void noah_lighting_slice_layer_status_collect(uint8_t *slice) {
    layer_ownership_lighting_projection(slice);
    slice[NOAH_LIGHTING_LAYER_TOP_OFFSET] = get_highest_layer(layer_state | 1u);
}
void noah_lighting_slice_layer_status_idle(uint8_t *slice) { memset(slice, 0, NOAH_LIGHTING_LAYER_STATUS_BYTES); slice[NOAH_LIGHTING_LAYER_ONESHOT_OFFSET] = 255u; }
bool noah_lighting_fact_layer_status_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    const uint8_t *slice = facts->layer_status;
    while (*cursor < LAYER_COUNT) {
        uint8_t layer = LAYER_COUNT - 1u - (*cursor)++;
        if (params[0] != 255u && params[0] != layer) continue;
        bool match = false;
        noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_LAYER, layer, 0u);
        for (uint8_t key = 0u; key < MATRIX_ROWS * MATRIX_COLS; key++) {
            if (slice[key] != layer + 1u) continue;
            key_origin_bitmap_add_keypos(subject->keys, (keypos_t){.row = key / MATRIX_COLS, .col = key % MATRIX_COLS});
        }
        subject->has_keys = key_origin_bitmap_has_any(subject->keys);
        subject->sides = key_origin_bitmap_side_mask(subject->keys);
        switch (params[1]) {
            case 0u: match = slice[NOAH_LIGHTING_LAYER_TOP_OFFSET] == layer; break;
            case 1u: match = subject->has_keys; break;
            case 2u: match = (slice[NOAH_LIGHTING_LAYER_LOCK_OFFSET + layer / 8u] & (1u << (layer % 8u))) != 0u; break;
            case 3u: match = slice[NOAH_LIGHTING_LAYER_ONESHOT_OFFSET] == layer; break;
        }
        if (match) return true;
    }
    return false;
}
#endif
