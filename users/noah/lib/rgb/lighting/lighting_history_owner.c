#include "lighting_history_owner.h"
#include "lighting_owned_batch.h"
#include "lighting_history_work.h"
#include <string.h>

static uint16_t lower_bound(const noah_lighting_history_owner_t *owner, uint8_t rule, uint8_t thing) {
    uint16_t first = 0u, end = owner->count;
    uint16_t key = (uint16_t)rule * 256u + thing;
    while (first < end) {
        NOAH_LIGHTING_HISTORY_WORK(NOAH_LIGHTING_WORK_SEARCH, 1u);
        uint16_t                        middle = first + (end - first) / 2u;
        const noah_lighting_retained_t *cell   = owner->cells + middle;
        uint16_t                        value  = (uint16_t)noah_lighting_retained_rule(cell) * 256u + noah_lighting_retained_thing(cell);
        if (value < key)
            first = middle + 1u;
        else
            end = middle;
    }
    return first;
}
const noah_lighting_retained_t *noah_lighting_history_owner_find(const noah_lighting_history_owner_t *owner, uint8_t rule, uint8_t thing) {
    uint16_t at = lower_bound(owner, rule, thing);
    if (at >= owner->count) return NULL;
    const noah_lighting_retained_t *cell = owner->cells + at;
    return noah_lighting_retained_rule(cell) == rule && noah_lighting_retained_thing(cell) == thing ? cell : NULL;
}
static noah_lighting_retained_t *record(void *context, uint8_t rule, uint8_t thing, noah_lighting_lifetime_parameters_t *timing) {
    noah_lighting_history_owner_t *owner = context;
    uint16_t                       slot;
    if (!noah_lighting_history_index_lookup(owner->program, owner->index, rule, thing, &slot, timing)) return NULL;
    return (noah_lighting_retained_t *)noah_lighting_history_owner_find(owner, rule, thing);
}
// Transport deduplicates committed packets before main consumption. Source
// tokens identify actual new occurrences. Once no render/delivery/matching
// ownership remains, an event cell's replay watermark is therefore redundant;
// frozen frame copies retain their own independent payload references.
static void reclaim(noah_lighting_history_owner_t *owner) {
    uint16_t write = 0u, removed_before = 0u;
    for (uint16_t read = 0u; read < owner->count; read++) {
        if (noah_lighting_retained_needed(owner->cells + read)) {
            if (write != read) {
                NOAH_LIGHTING_HISTORY_WORK(NOAH_LIGHTING_WORK_MOVE, sizeof *owner->cells);
                owner->cells[write] = owner->cells[read];
            }
            write++;
        } else {
            noah_lighting_retained_discard(owner->cells + read, owner->arena);
            if (read < owner->cursor) removed_before++;
        }
    }
    owner->cursor -= removed_before;
    owner->count = write;
    if (owner->cursor >= write) owner->cursor = 0u;
}
static uint16_t cell_key(const noah_lighting_retained_t *cell) {
    return (uint16_t)noah_lighting_retained_rule(cell) * 256u + noah_lighting_retained_thing(cell);
}
static bool prepare(void *context, const noah_lighting_event_group_t *group) {
    noah_lighting_history_owner_t *owner                                        = context;
    uint8_t                        missing[NOAH_LIGHTING_EVENT_RULE_MASK_BYTES] = {0};
    uint16_t                       needed                                       = 0u;
    for (uint16_t r = 0u; r < 255u; r++)
        if (group->qualified[r / 8u] & (1u << (r % 8u))) {
            noah_lighting_history_rule_t        description;
            noah_lighting_lifetime_parameters_t timing;
            uint16_t                            slot;
            if (!noah_lighting_history_rule(owner->program, (uint8_t)r, &description) || !description.event || description.thing_kind != group->thing_kind || !noah_lighting_history_index_lookup(owner->program, owner->index, (uint8_t)r, group->thing, &slot, &timing)) return false;
            if (!noah_lighting_history_owner_find(owner, (uint8_t)r, group->thing)) {
                missing[r / 8u] |= 1u << (r % 8u);
                needed++;
            }
        }
    if (needed > owner->capacity - owner->count) return false;
    if (!needed) return true;
    // Backward merge preserves unread old records; no bank-sized copy and no
    // per-rule whole-bank shifts. All reservations were checked before mutation.
    uint16_t old_count  = owner->count;
    uint16_t cursor_key = old_count ? cell_key(owner->cells + owner->cursor) : 0u;
    uint16_t read = old_count, write = old_count + needed;
    for (uint16_t r = 255u; r-- > 0u;)
        if (missing[r / 8u] & (1u << (r % 8u))) {
            uint16_t key = r * 256u + group->thing;
            while (read && cell_key(owner->cells + read - 1u) > key) {
                --write;
                --read;
                if (write != read) {
                    NOAH_LIGHTING_HISTORY_WORK(NOAH_LIGHTING_WORK_MOVE, sizeof *owner->cells);
                    owner->cells[write] = owner->cells[read];
                }
            }
            noah_lighting_retained_init(owner->cells + --write, (uint8_t)r, group->thing);
        }
    while (read) {
        --write;
        --read;
        if (write != read) {
            NOAH_LIGHTING_HISTORY_WORK(NOAH_LIGHTING_WORK_MOVE, sizeof *owner->cells);
            owner->cells[write] = owner->cells[read];
        }
    }
    owner->count  = old_count + needed;
    owner->cursor = old_count ? lower_bound(owner, (uint8_t)(cursor_key >> 8u), (uint8_t)cursor_key) : 0u;
    return true;
}
bool noah_lighting_history_owner_begin(noah_lighting_history_owner_t *owner, const noah_profile_lighting_v1_program_t *program, const noah_lighting_history_index_t *index, noah_lighting_retained_t *cells, uint16_t capacity, noah_lighting_batch_arena_t *arena) {
    if (!owner || !program || !program->valid || !index || !arena) return false;
    if (capacity && !cells) return false;
    *owner = (noah_lighting_history_owner_t){.program = program, .index = index, .cells = cells, .arena = arena, .capacity = capacity};
    return true;
}
void noah_lighting_history_owner_discard(noah_lighting_history_owner_t *owner) {
    for (uint16_t at = 0; at < owner->count; at++)
        noah_lighting_retained_discard(owner->cells + at, owner->arena);
    owner->cursor = owner->count = 0u;
}
void noah_lighting_history_owner_advance(noah_lighting_history_owner_t *owner, uint32_t elapsed) {
    uint16_t                     previous = UINT16_MAX;
    noah_lighting_history_rule_t description;
    for (uint16_t at = 0u; at < owner->count; at++) {
        uint8_t rule = noah_lighting_retained_rule(owner->cells + at);
        if (previous != rule) {
            (void)noah_lighting_history_rule(owner->program, rule, &description);
            previous = rule;
        }
        NOAH_LIGHTING_HISTORY_WORK(NOAH_LIGHTING_WORK_CALENDAR, 1u);
        (void)noah_lighting_retained_observe(owner->cells + at, owner->arena, &description.timing, elapsed, NOAH_LIGHTING_LIFETIME_IDLE, description.thing_kind, NULL, 0u, 0u, false);
    }
    reclaim(owner);
}
bool noah_lighting_history_owner_event(noah_lighting_history_owner_t *owner, const noah_lighting_event_group_t *group, bool source_delivery, uint8_t encoded[NOAH_LIGHTING_GROUP_BATCH_BYTES]) {
    bool applied = noah_lighting_retained_group_apply(owner->arena, group, source_delivery, encoded, prepare, record, owner);
    if (!applied) reclaim(owner); // newly reserved metadata is empty until commit
    return applied;
}
bool noah_lighting_history_owner_state(noah_lighting_history_owner_t *owner, uint8_t rule, uint8_t thing, uint8_t observation, const uint8_t *batch, uint16_t size, uint64_t entry_order) {
    noah_lighting_history_rule_t        description;
    noah_lighting_lifetime_parameters_t timing;
    uint16_t                            slot;
    if (!noah_lighting_history_rule(owner->program, rule, &description) || description.event || !noah_lighting_history_index_lookup(owner->program, owner->index, rule, thing, &slot, &timing) || (observation != NOAH_LIGHTING_LIFETIME_MATCH && observation != NOAH_LIGHTING_LIFETIME_DEPART)) return false;
    if (description.domain == 1u && description.selected == 255u) thing = 255u;
    noah_lighting_retained_t *cell = record(owner, rule, thing, &timing);
    if (!cell) {
        if (observation == NOAH_LIGHTING_LIFETIME_DEPART) return true;
        if (owner->count == owner->capacity) return false;
        uint16_t at = lower_bound(owner, rule, thing);
        NOAH_LIGHTING_HISTORY_WORK(NOAH_LIGHTING_WORK_MOVE, (owner->count - at) * sizeof *owner->cells);
        memmove(owner->cells + at + 1u, owner->cells + at, (owner->count - at) * sizeof *owner->cells);
        cell = owner->cells + at;
        noah_lighting_retained_init(cell, rule, thing);
        if (owner->count && at <= owner->cursor) owner->cursor++;
        owner->count++;
    }
    bool observed = noah_lighting_retained_observe(cell, owner->arena, &timing, 0u, observation, description.thing_kind, batch, size, entry_order, false);
    reclaim(owner);
    return observed;
}
uint32_t noah_lighting_history_owner_wake(const noah_lighting_history_owner_t *owner) {
    uint16_t                     previous = UINT16_MAX;
    noah_lighting_history_rule_t description;
    uint32_t                     wake = 0u;
    for (uint16_t at = 0u; at < owner->count; at++) {
        uint8_t rule = noah_lighting_retained_rule(owner->cells + at);
        if (previous != rule) {
            (void)noah_lighting_history_rule(owner->program, rule, &description);
            previous = rule;
        }
        uint32_t next = noah_lighting_retained_wake(owner->cells + at, &description.timing);
        if (next && (!wake || next < wake)) wake = next;
    }
    return wake;
}
int noah_lighting_history_owner_next(noah_lighting_history_owner_t *owner, noah_lighting_event_group_t *group) {
    uint16_t count = owner->count;
    for (uint16_t scanned = 0u; scanned < count; scanned++) {
        NOAH_LIGHTING_HISTORY_WORK(NOAH_LIGHTING_WORK_DIRTY, 1u);
        uint16_t at                    = owner->cursor;
        owner->cursor                  = (uint16_t)((at + 1u) % count);
        noah_lighting_retained_t *cell = owner->cells + at;
        if (!noah_lighting_retained_dirty(cell)) continue;
        noah_lighting_owned_batch_reader_t reader;
        noah_lighting_subject_t            subject;
        noah_lighting_batch_ref_t          reference = noah_lighting_retained_batch(cell);
        if (!noah_lighting_owned_batch_begin(owner->arena, reference, &reader) || reader.remaining != 1u || !noah_lighting_owned_batch_next(owner->arena, &reader, &subject) || reader.remaining || reader.offset != reader.size) return NOAH_LIGHTING_HISTORY_INVALID;
        *group = (noah_lighting_event_group_t){.order = noah_lighting_retained_order(cell), .thing_kind = subject.thing_kind, .thing = subject.thing, .slot = subject.slot, .outcome = subject.outcome, .sides = subject.sides, .has_keys = subject.has_keys};
        memcpy(group->owners, subject.keys, sizeof subject.keys);
        // Equality of full immutable payload reference plus captured order
        // identifies one occurrence. Lifetimes/dirty state stay per rule.
        for (uint16_t other = 0u; other < count; other++) {
            NOAH_LIGHTING_HISTORY_WORK(NOAH_LIGHTING_WORK_DIRTY, 1u);
            noah_lighting_retained_t *candidate = owner->cells + other;
            if (noah_lighting_retained_dirty(candidate) && noah_lighting_retained_batch(candidate) == reference && noah_lighting_retained_order(candidate) == group->order) {
                uint8_t rule = noah_lighting_retained_rule(candidate);
                group->qualified[rule / 8u] |= 1u << (rule % 8u);
            }
        }
        return NOAH_LIGHTING_HISTORY_READY;
    }
    return NOAH_LIGHTING_HISTORY_EMPTY;
}
void noah_lighting_history_owner_ack(noah_lighting_history_owner_t *owner, const noah_lighting_event_group_t *group) {
    for (uint16_t r = 0; r < owner->program->rule_count; r++)
        if (group->qualified[r / 8u] & (1u << (r % 8u))) {
            noah_lighting_lifetime_parameters_t timing;
            noah_lighting_retained_t           *cell = record(owner, (uint8_t)r, group->thing, &timing);
            if (cell) (void)noah_lighting_retained_ack(cell, owner->arena, group->order);
        }
    reclaim(owner);
}
void noah_lighting_history_owner_session(noah_lighting_history_owner_t *owner, bool source) {
    uint16_t                     previous = UINT16_MAX;
    noah_lighting_history_rule_t description;
    for (uint16_t at = 0u; at < owner->count; at++) {
        uint8_t rule = noah_lighting_retained_rule(owner->cells + at);
        if (previous != rule) {
            (void)noah_lighting_history_rule(owner->program, rule, &description);
            previous = rule;
        }
        if (!description.event) continue;
        if (source)
            noah_lighting_retained_seed(owner->cells + at, owner->arena);
        else
            noah_lighting_retained_discard(owner->cells + at, owner->arena);
    }
    reclaim(owner);
    owner->cursor = 0u;
}
