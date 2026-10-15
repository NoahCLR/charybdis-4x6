#include "lighting_lifetime.h"

enum { ABSENT, MATCHING, TAIL, FLASH, PHASE_MASK = 3u, ELIGIBLE = 4u };
static uint16_t age(const noah_lighting_lifetime_t *cell) {
    return (uint16_t)cell->bytes[0] | (uint16_t)cell->bytes[1] << 8u;
}
static void set(noah_lighting_lifetime_t *cell, uint16_t value, uint8_t flags) {
    cell->bytes[0] = (uint8_t)value;
    cell->bytes[1] = (uint8_t)(value >> 8u);
    cell->bytes[2] = flags;
}
uint8_t noah_lighting_lifetime_observe(noah_lighting_lifetime_t *cell, const noah_lighting_lifetime_parameters_t *parameters, uint32_t elapsed, uint8_t observation) {
    uint8_t  phase    = cell->bytes[2] & PHASE_MASK;
    uint16_t previous = age(cell);
    uint16_t current  = elapsed >= UINT16_MAX - (uint32_t)previous ? UINT16_MAX : (uint16_t)(previous + elapsed);
    uint8_t  action   = NOAH_LIGHTING_BATCH_KEEP;
    if (phase == ABSENT) current = 0u;
    if ((phase == TAIL && current >= parameters->linger) || (phase == FLASH && current >= parameters->flash)) {
        set(cell, 0u, ABSENT);
        phase   = ABSENT;
        current = 0u;
        action  = NOAH_LIGHTING_BATCH_RELEASE_RENDER;
    }
    if (observation == NOAH_LIGHTING_LIFETIME_EVENT) {
        set(cell, 0u, FLASH);
        return NOAH_LIGHTING_BATCH_REPLACE;
    }
    if (observation == NOAH_LIGHTING_LIFETIME_MATCH) {
        if (phase != MATCHING) current = 0u;
        bool eligible = current >= parameters->show_after;
        set(cell, current, MATCHING | (eligible ? ELIGIBLE : 0u));
        return eligible ? NOAH_LIGHTING_BATCH_REPLACE : NOAH_LIGHTING_BATCH_RELEASE_RENDER;
    }
    if (observation == NOAH_LIGHTING_LIFETIME_DEPART && phase == MATCHING) {
        // Advancing time at departure cannot mature a previously unmet gate.
        if ((cell->bytes[2] & ELIGIBLE) && parameters->linger) {
            set(cell, 0u, TAIL);
            return NOAH_LIGHTING_BATCH_KEEP;
        }
        set(cell, 0u, ABSENT);
        return NOAH_LIGHTING_BATCH_RELEASE_RENDER;
    }
    set(cell, current, cell->bytes[2]);
    return action;
}
bool noah_lighting_lifetime_visible(const noah_lighting_lifetime_t *cell, const noah_lighting_lifetime_parameters_t *parameters) {
    uint8_t phase = cell->bytes[2] & PHASE_MASK;
    if (phase == MATCHING) return (cell->bytes[2] & ELIGIBLE) != 0u;
    if (phase == TAIL) return age(cell) < parameters->linger;
    return phase == FLASH && age(cell) >= parameters->show_after && age(cell) < parameters->flash;
}
uint32_t noah_lighting_lifetime_wake(const noah_lighting_lifetime_t *cell, const noah_lighting_lifetime_parameters_t *parameters) {
    uint8_t  phase   = cell->bytes[2] & PHASE_MASK;
    uint16_t current = age(cell);
    if (phase == MATCHING && !(cell->bytes[2] & ELIGIBLE) && current < parameters->show_after) return parameters->show_after - current;
    if (phase == TAIL && current < parameters->linger) return parameters->linger - current;
    if (phase == FLASH && current < parameters->flash) {
        uint16_t boundary = parameters->show_after > current && parameters->show_after < parameters->flash ? parameters->show_after : parameters->flash;
        return boundary - current;
    }
    return 0u;
}

bool noah_lighting_lifetime_matching(const noah_lighting_lifetime_t *cell) {
    return (cell->bytes[2] & PHASE_MASK) == MATCHING;
}
