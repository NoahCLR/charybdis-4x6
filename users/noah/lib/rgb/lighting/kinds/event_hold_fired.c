// Optional occurrence matcher; this kind has no held/recent-event truth.
#include "../lighting_occurrence.h"
#if NOAH_LIGHTING_FACT_HOLD_FIRED
bool noah_lighting_event_hold_fired_match(const noah_lighting_event_t *event, const uint8_t *params, noah_lighting_subject_t *subject) {
    if (!noah_lighting_event_subject(event, NOAH_LIGHTING_EVENT_HOLD_FIRED, params[0], NOAH_LIGHTING_THING_BEHAVIOUR, 128u, subject)) return false;
    return true;
}
#endif
