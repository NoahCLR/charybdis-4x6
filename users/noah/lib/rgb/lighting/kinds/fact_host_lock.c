// Registered phase-3 fact: host lock.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_HOST_LOCK
#include "host.h"
void noah_lighting_slice_host_lock_collect(uint8_t *slice) {
    led_t leds = host_keyboard_led_state();
    slice[0] = (leds.caps_lock ? 1u : 0u) | (leds.num_lock ? 2u : 0u) | (leds.scroll_lock ? 4u : 0u);
}
void noah_lighting_slice_host_lock_idle(uint8_t *slice) { slice[0] = 0u; }
bool noah_lighting_fact_host_lock_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    if ((*cursor)++ || !(facts->host_lock[0] & (1u << params[0]))) return false;
    noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_BOARD, 255u, 0u);
    return true;
}
#endif
