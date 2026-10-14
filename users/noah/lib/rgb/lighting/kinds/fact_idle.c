// Registered phase-3 fact: idle.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_IDLE
#include "../../../profile/runtime/effective_settings_runtime.h"
void noah_lighting_slice_idle_collect(uint8_t *slice) {
    uint32_t timeout = noah_setting(NOAH_SETTING_RGB_TIMEOUT, 900000);
    slice[0] = timeout != 0u && last_input_activity_elapsed() > timeout;
}
void noah_lighting_slice_idle_idle(uint8_t *slice) { slice[0] = 0u; }
bool noah_lighting_fact_idle_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    (void)params;
    if ((*cursor)++ || !facts->idle[0]) return false;
    noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_BOARD, 255u, 0u);
    return true;
}
#endif
