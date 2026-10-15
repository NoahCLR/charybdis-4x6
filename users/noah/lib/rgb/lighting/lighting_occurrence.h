#pragma once
#include "lighting.h"
#include "lighting_events.h"
#include "lighting_event_protocol.h"

// Call synchronously against one coherent occurrence frame. No retained state,
// producer reentry, reader I/O or current-filter replay is hidden here.
bool noah_lighting_event_subject(const noah_lighting_event_t *event, uint8_t expected, uint8_t selector, uint8_t thing_kind, uint8_t domain, noah_lighting_subject_t *subject);
#define NOAH_LIGHTING_DECLARE_EVENT(id, name, thing, layout, slots, selector, signatures, live, role) bool noah_lighting_event_##name##_match(const noah_lighting_event_t *event, const uint8_t *params, noah_lighting_subject_t *subject);
NOAH_LIGHTING_EVENT_FACT_ROWS(NOAH_LIGHTING_DECLARE_EVENT)
#undef NOAH_LIGHTING_DECLARE_EVENT
bool noah_lighting_event_match(uint8_t kind, const noah_lighting_event_t *event, const uint8_t *params, noah_lighting_subject_t *subject);
bool noah_lighting_event_qualify(const noah_profile_lighting_v1_program_t *program, const noah_lighting_facts_t *facts, const noah_lighting_event_t *event, uint64_t order, noah_lighting_event_group_t *group);
