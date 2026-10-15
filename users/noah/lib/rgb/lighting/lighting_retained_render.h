#pragma once
#include "lighting_history_store.h"
#include "lighting_owned_batch.h"
#include "lighting_priority.h"

// The owning fact module supplies its unchanged natural priority. Paint/target
// selection remains with the caller; this merge never switches on kind IDs.
typedef uint64_t (*noah_lighting_natural_priority_fn)(void *context, const noah_lighting_subject_t *subject, uint64_t entry, uint8_t ordinal);
typedef void (*noah_lighting_retained_paint_fn)(void *context, const noah_lighting_subject_t *subject, const noah_lighting_priority_t *priority);
// Streams the COMPLETE retained domain bottom-up, without a64/128-subject
// placement cap or a second batch copy. No mutation/reentry during this call.
// Optional candidate counter measures scans only, not target time/cadence.
bool noah_lighting_retained_render(const noah_lighting_history_store_t *store, uint8_t type, uint8_t rule, const noah_lighting_lifetime_parameters_t *parameters, noah_lighting_natural_priority_fn natural, noah_lighting_retained_paint_fn paint, void *context, uint64_t *candidates);
