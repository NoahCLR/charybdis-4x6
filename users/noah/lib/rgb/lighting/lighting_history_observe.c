#include "lighting_history_observe.h"

#ifdef RGB_MATRIX_ENABLE

static bool holds(const noah_lighting_facts_t *facts, const uint8_t *condition) {
    noah_lighting_subject_t subject;
    uint16_t                cursor = 0u;
    bool                    any    = noah_lighting_fact_next(condition[1], facts, condition + 2u, &cursor, &subject);
    return condition[0] >> 5u == NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED ? !any : any;
}

static bool observe_rule(noah_lighting_history_bank_t *bank, const noah_lighting_facts_t *facts, uint8_t rule, uint64_t order, uint8_t *scratch) {
    noah_lighting_history_rule_t info;
    if (!noah_lighting_history_rule(bank->program, rule, &info)) return false;
    if (!info.retained || info.event) return true;
    const uint8_t *bytes = bank->program->bytes + bank->program->rules[rule];
    const uint8_t *at = bytes + 3u, *end = bytes + 1u + bytes[0];
    const uint8_t *first = NULL;
    if (at < end && at[0] >> 5u == NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION) {
        first = at;
        at += 2u + (at[0] & 31u);
    }
    bool matches = true;
    while (at < end && at[0] >> 5u <= NOAH_PROFILE_LIGHTING_V1_SLOT_NEGATED) {
        if (!holds(facts, at)) {
            matches = false;
            break;
        }
        at += 2u + (at[0] & 31u);
    }
    uint8_t present[16] = {0};
    if (matches && first) {
        noah_lighting_subject_t subject;
        uint16_t                cursor = 0u;
        while (noah_lighting_fact_next(first[1], facts, first + 2u, &cursor, &subject)) {
            uint8_t slot = info.domain == 1u ? 0u : subject.thing;
            if (slot >= info.domain) return false; // registry/producer invariant
            if (present[slot / 8u] & (1u << (slot % 8u))) continue;
            present[slot / 8u] |= 1u << (slot % 8u);
            uint8_t                      thing = subject.thing;
            noah_lighting_batch_writer_t writer;
            noah_lighting_batch_write_begin(&writer, scratch, NOAH_LIGHTING_BATCH_BYTES);
            // Retain the full last matching ordered batch for this thing, not
            // one key or an aggregate LED mask. Partial same-thing departure
            // refreshes this batch and does not start an independent tail.
            uint16_t members = 0u;
            while (noah_lighting_fact_next(first[1], facts, first + 2u, &members, &subject))
                if (subject.thing == thing && !noah_lighting_batch_write(&writer, &subject)) return false;
            if (!noah_lighting_history_bank_state(bank, rule, thing, NOAH_LIGHTING_LIFETIME_MATCH, scratch, writer.used, order)) return false;
        }
    } else if (matches) {
        noah_lighting_subject_t board;
        noah_lighting_subject_init(&board, NOAH_LIGHTING_THING_BOARD, 255u, 0u);
        noah_lighting_batch_writer_t writer;
        noah_lighting_batch_write_begin(&writer, scratch, NOAH_LIGHTING_BATCH_BYTES);
        if (!noah_lighting_batch_write(&writer, &board) || !noah_lighting_history_bank_state(bank, rule, 255u, NOAH_LIGHTING_LIFETIME_MATCH, scratch, writer.used, order)) return false;
        present[0] = 1u;
    }
    uint16_t                 after = UINT16_MAX;
    noah_lighting_retained_t cell;
    while (noah_lighting_history_store_rule_next(&bank->store, NOAH_LIGHTING_ARENA_CURRENT, rule, &after, &cell)) {
        uint8_t thing = noah_lighting_retained_thing(&cell), slot = info.domain == 1u ? 0u : thing;
        if (!(present[slot / 8u] & (1u << (slot % 8u))) && !noah_lighting_history_bank_state(bank, rule, thing, NOAH_LIGHTING_LIFETIME_DEPART, NULL, 0u, order)) return false;
    }
    return true;
}

bool noah_lighting_history_observe(noah_lighting_history_bank_t *bank, const noah_lighting_facts_t *facts, uint64_t order, uint8_t scratch[NOAH_LIGHTING_BATCH_BYTES]) {
    if (!bank || !bank->program || !bank->program->valid || !facts || !scratch) return false;
    for (uint16_t rule = 0u; rule < bank->program->rule_count; rule++)
        if (!observe_rule(bank, facts, (uint8_t)rule, order, scratch)) return false;
    return true;
}

#endif // RGB_MATRIX_ENABLE
