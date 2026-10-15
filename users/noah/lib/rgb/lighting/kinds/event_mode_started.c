// Optional occurrence matcher; this kind has no held/recent-event truth.
#include "../lighting_occurrence.h"
#if NOAH_LIGHTING_FACT_MODE_STARTED
bool noah_lighting_event_mode_started_match(const noah_lighting_event_t *event, const uint8_t *params, noah_lighting_subject_t *subject) {
    if (!noah_lighting_event_subject(event, NOAH_LIGHTING_EVENT_MODE_STARTED, params[0], NOAH_LIGHTING_THING_POINTING_MODE, 32u, subject)) return false;
    return true;
}
#endif
