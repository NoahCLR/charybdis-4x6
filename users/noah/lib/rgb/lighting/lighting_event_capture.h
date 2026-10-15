#pragma once
#include "lighting_occurrence.h"

// Synchronous capture pipeline with caller-owned scratch. The pipeline does
// not retain the fact frame or producer pointers. The sole history owner must
// consume/copy a qualified group before this call returns. Main only; neither
// collector nor consumer may reenter an input producer or publish a profile.
typedef void (*noah_lighting_event_collect_fn)(noah_lighting_facts_t *facts);
typedef void (*noah_lighting_event_retain_fn)(void *context, const noah_lighting_event_group_t *group, uint32_t now);
bool noah_lighting_event_capture(const noah_profile_lighting_v1_program_t *program, const noah_lighting_event_t *event, uint64_t order, uint32_t now, noah_lighting_facts_t *facts, noah_lighting_event_group_t *group, noah_lighting_event_collect_fn collect, noah_lighting_event_retain_fn retain, void *context);
