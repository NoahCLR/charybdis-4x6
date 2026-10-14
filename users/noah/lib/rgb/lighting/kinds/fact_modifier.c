// Registered phase-3 fact: modifier.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_MODIFIER

void noah_lighting_slice_modifier_collect(uint8_t *slice) {
    slice[0] = get_mods() | get_weak_mods();
    slice[1] = 0u;
#ifndef NO_ACTION_ONESHOT
    slice[0] |= get_oneshot_locked_mods();
    slice[1] = get_oneshot_mods();
#endif
}
void noah_lighting_slice_modifier_idle(uint8_t *slice) { slice[0] = slice[1] = 0u; }
bool noah_lighting_fact_modifier_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    uint8_t mods = facts->modifier[params[2]];
    while (*cursor < 2u) {
        uint8_t side = (*cursor)++;
        uint8_t bit = params[0] + side * 4u;
        if ((params[1] != 2u && params[1] != side) || !(mods & (1u << bit))) continue;
        noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_MODIFIER, bit, 0u);
        return true;
    }
    return false;
}
#endif
