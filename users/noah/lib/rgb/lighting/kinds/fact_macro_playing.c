// Registered phase-3 fact: macro playing.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_MACRO_PLAYING
#include "../../../macro/macro_payload.h"
void noah_lighting_slice_macro_playing_collect(uint8_t *slice) { slice[0] = macro_payload_playing_slot(); }
void noah_lighting_slice_macro_playing_idle(uint8_t *slice) { slice[0] = 255u; }
bool noah_lighting_fact_macro_playing_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    uint8_t slot = facts->macro_playing[0];
    if ((*cursor)++ || slot >= 128u || (params[0] != 255u && params[0] != slot)) return false;
    noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_MACRO, slot, 0u);
    return true;
}
#endif
