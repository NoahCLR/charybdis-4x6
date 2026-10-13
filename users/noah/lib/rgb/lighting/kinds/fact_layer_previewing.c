// Fact 2, layer previewing: the layer a pending momentary hold previews.
// Synced: the USB half's key runtime decides it.
#include "../lighting.h"

#if NOAH_LIGHTING_FACT_LAYER_PREVIEWING

void noah_lighting_slice_layer_previewing_collect(uint8_t *slice) {
    slice[0] = key_feedback_preview_layer();
}

void noah_lighting_slice_layer_previewing_idle(uint8_t *slice) {
    slice[0] = UINT8_MAX;
}

bool noah_lighting_fact_layer_previewing_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    uint8_t layer = facts->layer_previewing[0];

    if ((*cursor)++ != 0u || layer >= LAYER_COUNT || (params[0] != NOAH_PROFILE_LIGHTING_V1_ANY && params[0] != layer)) return false;
    noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_LAYER, layer, 0u);
    return true;
}

#endif
