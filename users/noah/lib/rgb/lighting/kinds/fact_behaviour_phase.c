// Registered phase-3 fact: behaviour phase.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_BEHAVIOUR_PHASE
#include "fact_key_subject.h"
void noah_lighting_slice_behaviour_phase_collect(uint8_t *slice) { key_feedback_lighting_phases(slice); }
void noah_lighting_slice_behaviour_phase_idle(uint8_t *slice) { memset(slice, 0, NOAH_LIGHTING_BEHAVIOUR_PHASE_BYTES); }
bool noah_lighting_fact_behaviour_phase_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    while (*cursor < MATRIX_ROWS * MATRIX_COLS) {
        uint8_t key = (*cursor)++;
        const uint8_t *record = facts->behaviour_phase + key * NOAH_LIGHTING_BEHAVIOUR_PHASE_RECORD_BYTES;
        if (!record[0] || !(record[1] & (1u << params[1])) || (params[0] != 255u && params[0] != record[0] - 1u) || (params[2] != 255u && params[2] != record[2])) continue;
        uint8_t slot = params[1] == 4u ? 1u : params[1] == 3u || params[1] == 2u ? 0u : params[1] == 1u ? 2u : 3u + (record[2] > 2u ? record[2] - 2u : 0u);
        noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_BEHAVIOUR, record[0] - 1u, slot);
        noah_lighting_subject_key(subject, key);
        return true;
    }
    return false;
}
#endif
