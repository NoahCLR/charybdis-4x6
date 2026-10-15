#include "lighting_retained_render.h"

bool noah_lighting_retained_render(const noah_lighting_history_store_t *store, uint8_t type, uint8_t rule, const noah_lighting_lifetime_parameters_t *parameters, noah_lighting_natural_priority_fn natural, noah_lighting_retained_paint_fn paint, void *context, uint64_t *candidates) {
    if (!store || !parameters || !natural || !paint) return false;
    noah_lighting_priority_t previous      = {0};
    bool                     have_previous = false;
    for (;;) {
        uint16_t                 after = UINT16_MAX;
        noah_lighting_retained_t cell;
        noah_lighting_priority_t selected         = {0};
        noah_lighting_subject_t  selected_subject = {0};
        bool                     found            = false;
        while (noah_lighting_history_store_rule_next(store, type, rule, &after, &cell)) {
            if (!noah_lighting_retained_visible(&cell, parameters)) continue;
            noah_lighting_owned_batch_reader_t reader;
            noah_lighting_subject_t            subject;
            if (!noah_lighting_owned_batch_begin(store->arena, noah_lighting_retained_batch(&cell), &reader)) return false;
            uint8_t ordinal = 0u;
            while (noah_lighting_owned_batch_next(store->arena, &reader, &subject)) {
                if (candidates) (*candidates)++;
                noah_lighting_priority_t priority = {.natural = natural(context, &subject, noah_lighting_retained_order(&cell), ordinal), .entry = noah_lighting_retained_order(&cell), .thing_kind = subject.thing_kind, .thing = subject.thing, .ordinal = ordinal};
                ordinal++;
                if (have_previous && !noah_lighting_priority_before(&priority, &previous)) continue;
                if (!found || noah_lighting_priority_before(&selected, &priority)) {
                    selected         = priority;
                    selected_subject = subject;
                    found            = true;
                }
            }
            if (reader.remaining || reader.offset != reader.size) return false;
        }
        if (!found) return true;
        paint(context, &selected_subject, &selected);
        previous      = selected;
        have_previous = true;
    }
}
