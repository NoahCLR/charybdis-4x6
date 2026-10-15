// Optional occurrence matcher; this kind has no held/recent-event truth.
#include "../lighting_occurrence.h"
#if NOAH_LIGHTING_FACT_TAP_FIRED
bool noah_lighting_event_tap_fired_match(const noah_lighting_event_t *event, const uint8_t *params, noah_lighting_subject_t *subject) {
    if (!noah_lighting_event_subject(event, NOAH_LIGHTING_EVENT_TAP_FIRED, params[0], NOAH_LIGHTING_THING_BEHAVIOUR, 128u, subject)) return false;
    if (!event->branch || (params[1] != 255u && params[1] != event->branch)) return false;
    subject->slot = event->branch == 1u ? NOAH_LIGHTING_BEHAVIOUR_SLOT_TAP : (uint8_t)(event->branch + 1u);
    return true;
}
#endif
