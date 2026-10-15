#include "lighting_occurrence.h"
#include <string.h>

#ifdef RGB_MATRIX_ENABLE
bool noah_lighting_event_subject(const noah_lighting_event_t *event, uint8_t expected, uint8_t selector, uint8_t thing_kind, uint8_t domain, noah_lighting_subject_t *subject) {
    if (!event || event->kind != expected || event->thing >= domain || (selector != 255u && selector != event->thing)) return false;
    noah_lighting_subject_init(subject, thing_kind, event->thing, event->slot);
    subject->outcome = event->outcome;
    if (event->owners) key_origin_bitmap_copy(subject->keys, event->owners);
    else if (key_origin_keypos_valid(event->key)) key_origin_bitmap_add_keypos(subject->keys, event->key);
    subject->has_keys = key_origin_bitmap_has_any(subject->keys);
    subject->sides = key_origin_bitmap_side_mask(subject->keys);
    return true;
}

bool noah_lighting_event_match(uint8_t kind, const noah_lighting_event_t *event, const uint8_t *params, noah_lighting_subject_t *subject) {
    (void)event; (void)params; (void)subject;
    switch (kind) {
#define NOAH_LIGHTING_MATCH_EVENT(id, name, thing, layout, slots, selector, signatures, live, role) case id: return noah_lighting_event_##name##_match(event, params, subject);
        NOAH_LIGHTING_EVENT_FACT_ROWS(NOAH_LIGHTING_MATCH_EVENT)
#undef NOAH_LIGHTING_MATCH_EVENT
        default: return false;
    }
}

bool noah_lighting_event_qualify(const noah_profile_lighting_v1_program_t *program, const noah_lighting_facts_t *facts, const noah_lighting_event_t *event, uint64_t order, noah_lighting_event_group_t *group) {
    if (!program || !program->valid || !facts || !event || !group) return false;
    memset(group, 0, sizeof *group);
    group->order = order;
    bool qualified = false;
    for (uint16_t index = 0u; index < program->rule_count; index++) {
        const uint8_t *rule = program->bytes + program->rules[index];
        const uint8_t *at = rule + 3u;
        const uint8_t *end = rule + 1u + rule[0];
        if ((rule[1] & NOAH_PROFILE_LIGHTING_V1_RULE_DISABLED) || at >= end || (at[0] >> 5u) != NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION || noah_lighting_fact_role(at[1]) != NOAH_LIGHTING_FACT_EVENT) continue;
        noah_lighting_subject_t subject;
        if (!noah_lighting_event_match(at[1], event, at + 2u, &subject)) continue;
        // Matching event modules describe the same captured occurrence. Stage
        // its one payload before reusing the local subject for state filters;
        // only qualified bits commit membership. A false result is unusable.
        group->thing_kind = subject.thing_kind;
        group->thing = subject.thing;
        group->slot = subject.slot;
        group->outcome = subject.outcome;
        group->sides = subject.sides;
        group->has_keys = subject.has_keys;
        memcpy(group->owners, subject.keys, sizeof subject.keys);
        at += 2u + (at[0] & 31u);
        bool holds = true;
        while (at < end && (at[0] >> 5u) <= NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED) {
            uint16_t cursor = 0u;
            bool any = noah_lighting_fact_next(at[1], facts, at + 2u, &cursor, &subject);
            if ((at[0] >> 5u) == NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED ? any : !any) { holds = false; break; }
            at += 2u + (at[0] & 31u);
        }
        if (!holds) continue;
        group->qualified[index / 8u] |= 1u << (index % 8u);
        qualified = true;
    }
    return qualified;
}
#endif
