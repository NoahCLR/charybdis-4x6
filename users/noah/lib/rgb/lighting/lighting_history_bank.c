#include "lighting_history_bank.h"
#include "lighting_owned_batch.h"
#include <string.h>

static bool description(const noah_lighting_history_bank_t *bank, uint8_t rule, uint8_t thing, noah_lighting_history_rule_t *value) {
    if (!noah_lighting_history_rule(bank->program, rule, value) || !value->retained) return false;
    return value->domain == 1u || (thing < value->domain && (value->selected == 255u || value->selected == thing));
}
static bool borrow(noah_lighting_history_bank_t *bank, uint8_t rule, uint8_t thing, noah_lighting_retained_t *cell) {
    if (!noah_lighting_history_store_read(&bank->store, NOAH_LIGHTING_ARENA_CURRENT, rule, thing, cell)) {
        noah_lighting_retained_init(cell, rule, thing);
        return true;
    }
    noah_lighting_batch_ref_t ref = noah_lighting_retained_batch(cell);
    return !ref || noah_lighting_batch_retain(bank->store.arena, ref);
}
static bool commit(noah_lighting_history_bank_t *bank, noah_lighting_retained_t *cell) {
    if (noah_lighting_history_store_write(&bank->store, cell)) return true;
    noah_lighting_retained_discard(cell, bank->store.arena);
    return false;
}
void noah_lighting_history_bank_begin(noah_lighting_history_bank_t *bank, const noah_profile_lighting_v1_program_t *program, noah_lighting_batch_arena_t *arena) {
    *bank = (noah_lighting_history_bank_t){.program = program};
    noah_lighting_history_store_init(&bank->store, arena);
}
bool noah_lighting_history_bank_event(noah_lighting_history_bank_t *bank, const noah_lighting_event_group_t *group, bool source, uint8_t encoded[1u + 3u + KEY_ORIGIN_BITMAP_SIZE]) {
    if (!group || group->qualified[31] & 0x80u) return false;
    uint8_t rules[32];
    memcpy(rules, group->qualified, sizeof rules);
    noah_lighting_history_rule_t first = {0};
    bool                         any   = false;
    for (uint16_t rule = 0u; rule < 255u; rule++)
        if (rules[rule / 8u] & (1u << (rule % 8u))) {
            noah_lighting_history_rule_t info;
            if (!description(bank, (uint8_t)rule, group->thing, &info) || !info.event || info.thing_kind != group->thing_kind) return false;
            noah_lighting_retained_t previous;
            if (noah_lighting_history_store_read(&bank->store, NOAH_LIGHTING_ARENA_CURRENT, (uint8_t)rule, group->thing, &previous) && noah_lighting_retained_order(&previous) == group->order)
                rules[rule / 8u] &= (uint8_t)~(1u << (rule % 8u));
            else {
                first = info;
                any   = true;
            }
        }
    if (!any) return true;
    noah_lighting_subject_t subject;
    noah_lighting_subject_init(&subject, group->thing_kind, group->thing, group->slot);
    subject.outcome  = group->outcome;
    subject.sides    = group->sides;
    subject.has_keys = group->has_keys;
    memcpy(subject.keys, group->owners, sizeof subject.keys);
    noah_lighting_batch_writer_t writer;
    noah_lighting_batch_write_begin(&writer, encoded, 1u + 3u + KEY_ORIGIN_BITMAP_SIZE);
    if (!noah_lighting_batch_write(&writer, &subject)) return false;
    noah_lighting_batch_ref_t ref;
    if (!noah_lighting_batch_acquire(bank->store.arena, subject.thing_kind, subject.thing, encoded, writer.used, &ref, NULL)) return false;
    noah_lighting_retained_t cell;
    noah_lighting_retained_init(&cell, 0u, subject.thing);
    noah_lighting_retained_install_event(&cell, bank->store.arena, &first.timing, ref, group->order, source);
    if (noah_lighting_history_store_group(&bank->store, &cell, rules)) return true;
    noah_lighting_retained_discard(&cell, bank->store.arena);
    return false;
}
bool noah_lighting_history_bank_state(noah_lighting_history_bank_t *bank, uint8_t rule, uint8_t thing, uint8_t observation, const uint8_t *batch, uint16_t size, uint64_t order) {
    noah_lighting_history_rule_t info;
    if (!description(bank, rule, thing, &info) || info.event || (observation != NOAH_LIGHTING_LIFETIME_MATCH && observation != NOAH_LIGHTING_LIFETIME_DEPART)) return false;
    if (info.domain == 1u && info.selected == 255u) thing = 255u;
    noah_lighting_retained_t cell;
    if (!borrow(bank, rule, thing, &cell)) return false;
    if (!noah_lighting_retained_observe(&cell, bank->store.arena, &info.timing, 0u, observation, info.thing_kind, batch, size, order, false)) {
        noah_lighting_retained_discard(&cell, bank->store.arena);
        return false;
    }
    return commit(bank, &cell);
}
bool noah_lighting_history_bank_advance(noah_lighting_history_bank_t *bank, uint32_t elapsed) {
    noah_lighting_history_cursor_t cursor;
    noah_lighting_history_store_cursor(&bank->store, &cursor);
    noah_lighting_retained_t cell;
    while (noah_lighting_history_store_next(&bank->store, NOAH_LIGHTING_ARENA_CURRENT, &cursor, &cell)) {
        noah_lighting_history_rule_t info;
        if (!description(bank, noah_lighting_retained_rule(&cell), noah_lighting_retained_thing(&cell), &info)) return false;
        noah_lighting_batch_ref_t ref = noah_lighting_retained_batch(&cell);
        if (ref && !noah_lighting_batch_retain(bank->store.arena, ref)) return false;
        if (!noah_lighting_retained_observe(&cell, bank->store.arena, &info.timing, elapsed, NOAH_LIGHTING_LIFETIME_IDLE, info.thing_kind, NULL, 0u, 0u, false)) return false;
        if (!commit(bank, &cell)) return false;
    }
    return true;
}
uint32_t noah_lighting_history_bank_wake(const noah_lighting_history_bank_t *bank) {
    noah_lighting_history_cursor_t cursor;
    noah_lighting_history_store_cursor(&bank->store, &cursor);
    noah_lighting_retained_t cell;
    uint32_t                 result = 0u;
    while (noah_lighting_history_store_next(&bank->store, NOAH_LIGHTING_ARENA_CURRENT, &cursor, &cell)) {
        noah_lighting_history_rule_t info;
        if (!description(bank, noah_lighting_retained_rule(&cell), noah_lighting_retained_thing(&cell), &info)) continue;
        uint32_t wake = noah_lighting_retained_wake(&cell, &info.timing);
        if (wake && (!result || wake < result)) result = wake;
    }
    return result;
}
bool noah_lighting_history_bank_next(noah_lighting_history_bank_t *bank, noah_lighting_event_group_t *group) {
    // Fair logical-key cursor remains stable across cohort splits/compaction.
    noah_lighting_history_cursor_t cursor = {.after = bank->dirty_started ? bank->dirty_after : UINT32_MAX};
    noah_lighting_retained_t       cell;
    bool                           found = false;
    for (uint8_t pass = 0u; pass < 2u && !found; pass++) {
        while (noah_lighting_history_store_next(&bank->store, NOAH_LIGHTING_ARENA_CURRENT, &cursor, &cell))
            if (noah_lighting_retained_dirty(&cell)) {
                found = true;
                break;
            }
        if (!found) cursor.after = UINT32_MAX;
    }
    if (!found) return false;
    bank->dirty_after   = (uint16_t)cursor.after;
    bank->dirty_started = true;
    noah_lighting_owned_batch_reader_t reader;
    noah_lighting_subject_t            subject;
    noah_lighting_batch_ref_t          ref = noah_lighting_retained_batch(&cell);
    if (!noah_lighting_owned_batch_begin(bank->store.arena, ref, &reader) || reader.remaining != 1u || !noah_lighting_owned_batch_next(bank->store.arena, &reader, &subject)) return false;
    *group = (noah_lighting_event_group_t){.order = noah_lighting_retained_order(&cell), .thing_kind = subject.thing_kind, .thing = subject.thing, .slot = subject.slot, .outcome = subject.outcome, .sides = subject.sides, .has_keys = subject.has_keys};
    memcpy(group->owners, subject.keys, sizeof subject.keys);
    noah_lighting_history_store_cursor(&bank->store, &cursor);
    while (noah_lighting_history_store_next(&bank->store, NOAH_LIGHTING_ARENA_CURRENT, &cursor, &cell))
        if (noah_lighting_retained_dirty(&cell) && noah_lighting_retained_batch(&cell) == ref && noah_lighting_retained_order(&cell) == group->order) {
            uint8_t rule = noah_lighting_retained_rule(&cell);
            group->qualified[rule / 8u] |= 1u << (rule % 8u);
        }
    return true;
}
bool noah_lighting_history_bank_ack(noah_lighting_history_bank_t *bank, const noah_lighting_event_group_t *group) {
    for (uint16_t rule = 0u; rule < 255u; rule++)
        if (group->qualified[rule / 8u] & (1u << (rule % 8u))) {
            noah_lighting_retained_t cell;
            if (!borrow(bank, (uint8_t)rule, group->thing, &cell)) return false;
            (void)noah_lighting_retained_ack(&cell, bank->store.arena, group->order);
            // Removing an ACKed member never grows its source cohort; splitting a
            // still-rendered member may need reserve and is accounted by the owner.
            if (!commit(bank, &cell)) return false;
        }
    return true;
}
bool noah_lighting_history_bank_session(noah_lighting_history_bank_t *bank, bool source) {
    noah_lighting_history_cursor_t cursor;
    noah_lighting_history_store_cursor(&bank->store, &cursor);
    noah_lighting_retained_t cell;
    while (noah_lighting_history_store_next(&bank->store, NOAH_LIGHTING_ARENA_CURRENT, &cursor, &cell)) {
        noah_lighting_history_rule_t info;
        if (!description(bank, noah_lighting_retained_rule(&cell), noah_lighting_retained_thing(&cell), &info) || !info.event) continue;
        if (!borrow(bank, noah_lighting_retained_rule(&cell), noah_lighting_retained_thing(&cell), &cell)) return false;
        if (source)
            noah_lighting_retained_seed(&cell, bank->store.arena);
        else
            noah_lighting_retained_discard(&cell, bank->store.arena);
        if (!commit(bank, &cell)) return false;
    }
    bank->dirty_started = false;
    return true;
}
