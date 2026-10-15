#include "lighting_history_index.h"
#include "lighting_history_work.h"
#include <string.h>

#define NOAH_LIGHTING_HISTORY_BOUND(id, name, thing, layout, slots, selector, signatures, live, role) _Static_assert((slots) <= 128u, "existing registry subject domain bound");
NOAH_LIGHTING_FACT_ROWS(NOAH_LIGHTING_HISTORY_BOUND)
#undef NOAH_LIGHTING_HISTORY_BOUND
_Static_assert(255u * 128u <= UINT16_MAX, "accepted rule/thing prefix bound fits u16");

static void subject_domain(uint8_t kind, uint8_t *thing, uint8_t *domain, uint8_t *selector) {
    switch (kind) {
#define NOAH_LIGHTING_HISTORY_DOMAIN(id, name, thing_kind, layout, slots, parameter, signatures, live, role) \
    case id:                                                                                                 \
        *thing    = thing_kind;                                                                              \
        *domain   = slots;                                                                                   \
        *selector = parameter;                                                                               \
        return;
        NOAH_LIGHTING_FACT_ROWS(NOAH_LIGHTING_HISTORY_DOMAIN)
#undef NOAH_LIGHTING_HISTORY_DOMAIN
        default:
            *thing    = 0u;
            *domain   = 1u;
            *selector = 255u;
            return;
    }
}
static void timing_parameters(uint8_t kind, const uint8_t *params, noah_lighting_lifetime_parameters_t *timing) {
    (void)params;
    (void)timing;
    switch (kind) {
#define NOAH_LIGHTING_HISTORY_TIMING(id, name, layout, timed, role) \
    case id:                                                        \
        noah_lighting_timing_##name##_lifetime(params, timing);     \
        return;
        NOAH_LIGHTING_TIMING_ROWS(NOAH_LIGHTING_HISTORY_TIMING)
#undef NOAH_LIGHTING_HISTORY_TIMING
        default:
            return;
    }
}
bool noah_lighting_history_rule(const noah_profile_lighting_v1_program_t *program, uint8_t rule, noah_lighting_history_rule_t *description) {
    if (!program || !program->valid || rule >= program->rule_count || !description) return false;
    NOAH_LIGHTING_HISTORY_WORK(NOAH_LIGHTING_WORK_DESCRIBE, 1u);
    *description         = (noah_lighting_history_rule_t){.domain = 1u, .selected = 255u};
    const uint8_t *bytes = program->bytes + program->rules[rule];
    if (bytes[1] & NOAH_PROFILE_LIGHTING_V1_RULE_DISABLED) return true;
    const uint8_t *at = bytes + 3u, *end = bytes + 1u + bytes[0];
    if (at < end && (at[0] >> 5u) == NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION) {
        uint8_t selector;
        description->kind = at[1];
        subject_domain(at[1], &description->thing_kind, &description->domain, &selector);
        if (selector != 255u) description->selected = at[2u + selector];
        description->event = noah_lighting_fact_role(at[1]) == NOAH_LIGHTING_FACT_EVENT;
    }
    for (; at < end; at += 2u + (at[0] & 31u))
        if ((at[0] >> 5u) == NOAH_PROFILE_LIGHTING_V1_SLOT_TIMING) timing_parameters(at[1], at + 2u, &description->timing);
    description->retained = description->timing.linger || description->timing.flash;
    return true;
}
static uint16_t count(const noah_lighting_history_rule_t *description) {
    return !description->retained ? 0u : description->selected == 255u ? description->domain : 1u;
}
bool noah_lighting_history_index_prepare(const noah_profile_lighting_v1_program_t *program, noah_lighting_history_index_t *index) {
    if (!program || !program->valid || !index) return false;
    memset(index, 0, sizeof *index);
    uint32_t total = 0u;
    for (uint16_t rule = 0u; rule < program->rule_count; rule++) {
        noah_lighting_history_rule_t description;
        if (!noah_lighting_history_rule(program, (uint8_t)rule, &description)) return false;
        total += count(&description);
        if (total > UINT16_MAX) return false; // registry/compiler invariant, not profile rejection
        index->offsets[rule + 1u] = (uint16_t)total;
    }
    return true;
}
bool noah_lighting_history_index_lookup(const noah_profile_lighting_v1_program_t *program, const noah_lighting_history_index_t *index, uint8_t rule, uint8_t thing, uint16_t *slot, noah_lighting_lifetime_parameters_t *timing) {
    noah_lighting_history_rule_t description;
    if (!index || !slot || !timing || !noah_lighting_history_rule(program, rule, &description) || !description.retained) return false;
    uint16_t offset = 0u;
    if (description.domain > 1u) {
        if (thing >= description.domain || (description.selected != 255u && description.selected != thing)) return false;
        if (description.selected == 255u) offset = thing;
    }
    *slot   = index->offsets[rule] + offset;
    *timing = description.timing;
    return true;
}
