#pragma once
#include "lighting_retained.h"
#include "lighting_event_protocol.h"
#include "lighting_batch.h"

// Main-only owner interface. Prepare reserves all required record slots without
// changing visible existing cells. Lookup is a stable, RAM-only lookup through
// the entire call; it may not render, capture input, publish/reset or do I/O.
// The production owner must bound record/payload storage for accepted profiles.
typedef bool (*noah_lighting_group_prepare_fn)(void *context, const noah_lighting_event_group_t *group);
typedef noah_lighting_retained_t *(*noah_lighting_group_record_fn)(void *context, uint8_t rule, uint8_t thing, noah_lighting_lifetime_parameters_t *parameters);
enum { NOAH_LIGHTING_GROUP_BATCH_BYTES = 1u + 3u + KEY_ORIGIN_BITMAP_SIZE };
// Input group and output workspace coexist and are caller-owned. Preflight and
// reserve happen before ANY visible cell changes; commit has no fallible calls.
bool noah_lighting_retained_group_apply(noah_lighting_batch_arena_t *arena, const noah_lighting_event_group_t *group, bool source_delivery, uint8_t encoded[NOAH_LIGHTING_GROUP_BATCH_BYTES], noah_lighting_group_prepare_fn prepare, noah_lighting_group_record_fn record, void *context);
