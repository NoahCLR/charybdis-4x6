// Fact 1, layer active: each active layer, highest first. Layer 0, the base
// layer, is always active. Local: both halves share QMK's layer state.
#include "../lighting.h"

#if NOAH_LIGHTING_FACT_LAYER_ACTIVE

#    include "fact_layer_active.h"

static uint16_t active_mask(const noah_lighting_facts_t *facts) {
    return (uint16_t)(facts->layer_active[0] | (uint16_t)facts->layer_active[1] << 8u | 1u);
}

void noah_lighting_slice_layer_active_collect(uint8_t *slice) {
    slice[0] = (uint8_t)layer_state;
    slice[1] = (uint8_t)(layer_state >> 8u);
}

void noah_lighting_slice_layer_active_idle(uint8_t *slice) {
    slice[0] = 1u;
    slice[1] = 0u;
}

bool noah_lighting_fact_layer_active_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    uint16_t mask = active_mask(facts);

    while (*cursor < LAYER_COUNT) {
        uint8_t layer = (uint8_t)(LAYER_COUNT - 1u - *cursor);
        (*cursor)++;
        if (((mask >> layer) & 1u) == 0u || (params[0] != NOAH_PROFILE_LIGHTING_V1_ANY && params[0] != layer)) continue;
        noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_LAYER, layer, 0u);
        return true;
    }
    return false;
}

bool noah_lighting_fact_layer_active_has(const noah_lighting_facts_t *facts, uint8_t layer) {
    return layer < 16u && ((active_mask(facts) >> layer) & 1u) != 0u;
}

void noah_lighting_fact_layer_active_without(noah_lighting_facts_t *facts, uint8_t layer) {
    if (layer < 16u) facts->layer_active[layer / 8u] &= (uint8_t)~(1u << (layer % 8u));
}

#endif
