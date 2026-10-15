#include "users/noah/lib/rgb/lighting/lighting_history_store.h"
#include "users/noah/lib/rgb/lighting/lighting_retained_group.h"
#include "users/noah/lib/rgb/lighting/lighting_retained_render.h"
#include "users/noah/lib/rgb/lighting/lighting_owned_batch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t                             bank[32768];
static noah_lighting_batch_arena_t         arena;
static noah_lighting_history_store_t       store;
static noah_lighting_lifetime_parameters_t duration = {.flash = 500};
static void                                event(uint8_t rule, uint8_t thing, uint64_t order, uint8_t key) {
    noah_lighting_retained_t cell;
    if (noah_lighting_history_store_read(&store, NOAH_LIGHTING_ARENA_CURRENT, rule, thing, &cell)) {
        noah_lighting_batch_ref_t ref = noah_lighting_retained_batch(&cell);
        if (ref) assert(noah_lighting_batch_retain(&arena, ref));
    } else
        noah_lighting_retained_init(&cell, rule, thing);
    uint8_t                      encoded[NOAH_LIGHTING_GROUP_BATCH_BYTES];
    noah_lighting_batch_writer_t writer;
    noah_lighting_batch_write_begin(&writer, encoded, sizeof encoded);
    noah_lighting_subject_t subject;
    noah_lighting_subject_init(&subject, 5, thing, key);
    subject.outcome  = key + 1u;
    subject.has_keys = true;
    subject.sides    = key < 30 ? 1 : 2;
    subject.keys[key / 8] |= 1u << (key % 8);
    assert(noah_lighting_batch_write(&writer, &subject));
    assert(noah_lighting_retained_observe(&cell, &arena, &duration, 0, NOAH_LIGHTING_LIFETIME_EVENT, 5, encoded, writer.used, order, true));
    assert(noah_lighting_history_store_write(&store, &cell));
}
static void advance(uint32_t elapsed) {
    noah_lighting_history_cursor_t cursor;
    noah_lighting_retained_t       cell;
    noah_lighting_history_store_cursor(&store, &cursor);
    while (noah_lighting_history_store_next(&store, NOAH_LIGHTING_ARENA_CURRENT, &cursor, &cell)) {
        noah_lighting_batch_ref_t ref = noah_lighting_retained_batch(&cell);
        if (ref) assert(noah_lighting_batch_retain(&arena, ref));
        assert(noah_lighting_retained_observe(&cell, &arena, &duration, elapsed, NOAH_LIGHTING_LIFETIME_IDLE, 5, NULL, 0, 0, false));
        assert(noah_lighting_history_store_write(&store, &cell));
    }
}
static unsigned cohorts(uint8_t type) {
    unsigned                  count  = 0;
    uint32_t                  offset = 0;
    noah_lighting_batch_ref_t ref;
    while (noah_lighting_record_next(&arena, type, &offset, &ref))
        count++;
    return count;
}
static noah_lighting_retained_t read(uint8_t type, uint8_t rule, uint8_t thing) {
    noah_lighting_retained_t cell;
    assert(noah_lighting_history_store_read(&store, type, rule, thing, &cell));
    return cell;
}
static unsigned paints;
static uint64_t last_entry;
static uint64_t captured(void *context, const noah_lighting_subject_t *subject, uint64_t entry, uint8_t ordinal) {
    (void)context;
    (void)subject;
    (void)ordinal;
    return UINT64_MAX - entry;
}
static void draw_all(void *context, const noah_lighting_subject_t *subject, const noah_lighting_priority_t *priority) {
    (void)context;
    (void)subject;
    assert(priority->entry >= last_entry);
    last_entry = priority->entry;
    paints++;
}
int main(void) {
    noah_lighting_batch_arena_init(&arena, bank, sizeof bank);
    noah_lighting_history_store_init(&store, &arena);
    event(254, 127, 1, 59);
    unsigned first = arena.used;
    for (unsigned rule = 0; rule < 254; rule++)
        event(rule, 127, 1, 59);
    assert(cohorts(NOAH_LIGHTING_ARENA_CURRENT) == 1);
    assert(arena.used == 80u); // batch17 + full template/mask/header63
    assert(first == 49u);      // batch17 + singleton metadata32
    assert(noah_lighting_history_store_freeze(&store));
    assert(arena.used == 143u);
    uint32_t frozen_peak = arena.used;
    event(0, 127, 2, 0);
    assert(cohorts(NOAH_LIGHTING_ARENA_CURRENT) == 2);
    noah_lighting_retained_t old = read(NOAH_LIGHTING_ARENA_FRAME, 0, 127), new = read(NOAH_LIGHTING_ARENA_CURRENT, 0, 127), other = read(NOAH_LIGHTING_ARENA_CURRENT, 254, 127);
    assert(noah_lighting_retained_order(&old) == 1 && noah_lighting_retained_order(&other) == 1 && noah_lighting_retained_order(&new) == 2);
    advance(499);
    new = read(NOAH_LIGHTING_ARENA_CURRENT, 0, 127);
    assert(noah_lighting_retained_visible(&new, &duration));
    old = read(NOAH_LIGHTING_ARENA_FRAME, 0, 127);
    assert(noah_lighting_retained_wake(&old, &duration) == 500);
    advance(1);
    new = read(NOAH_LIGHTING_ARENA_CURRENT, 0, 127);
    assert(!noah_lighting_retained_visible(&new, &duration) && noah_lighting_retained_dirty(&new));
    // Old ACK must not affect newer qualifying rule0; the others can independently
    // ACK and disappear without releasing the frozen cohort's payload.
    for (unsigned rule = 0; rule < 255; rule++) {
        noah_lighting_retained_t  cell = read(NOAH_LIGHTING_ARENA_CURRENT, rule, 127);
        noah_lighting_batch_ref_t ref  = noah_lighting_retained_batch(&cell);
        assert(noah_lighting_batch_retain(&arena, ref));
        (void)noah_lighting_retained_ack(&cell, &arena, 1);
        assert(noah_lighting_history_store_write(&store, &cell));
    }
    assert(cohorts(NOAH_LIGHTING_ARENA_CURRENT) == 1);
    new = read(NOAH_LIGHTING_ARENA_CURRENT, 0, 127);
    assert(noah_lighting_retained_dirty(&new));
    noah_lighting_owned_batch_reader_t reader;
    noah_lighting_subject_t            subject;
    assert(noah_lighting_owned_batch_begin(&arena, noah_lighting_retained_batch(&old), &reader));
    assert(noah_lighting_owned_batch_next(&arena, &reader, &subject));
    assert(subject.keys[7] == 8 && subject.slot == 59 && subject.outcome == 60);
    noah_lighting_history_store_clear(&store, NOAH_LIGHTING_ARENA_CURRENT);
    assert(cohorts(NOAH_LIGHTING_ARENA_FRAME) == 1 && arena.used == 80u);
    noah_lighting_history_store_clear(&store, NOAH_LIGHTING_ARENA_FRAME);
    assert(!arena.used);
    // Failed split/cohort expansion and failed complete-frame reservation leave
    // the exact old cell and full payload reference counts intact.
    event(0, 127, 3, 1);
    event(1, 127, 3, 1);
    uint32_t capacity = arena.capacity;
    arena.capacity    = arena.used;
    assert(!noah_lighting_history_store_freeze(&store));
    assert(cohorts(NOAH_LIGHTING_ARENA_FRAME) == 0);
    noah_lighting_retained_t  cell = read(NOAH_LIGHTING_ARENA_CURRENT, 0, 127), before = cell;
    noah_lighting_batch_ref_t ref = noah_lighting_retained_batch(&cell);
    assert(noah_lighting_batch_retain(&arena, ref));
    assert(noah_lighting_retained_ack(&cell, &arena, 3));
    assert(!noah_lighting_history_store_write(&store, &cell));
    noah_lighting_retained_discard(&cell, &arena);
    cell = read(NOAH_LIGHTING_ARENA_CURRENT, 0, 127);
    assert(!memcmp(&cell, &before, sizeof cell));
    arena.capacity = capacity;
    noah_lighting_history_store_clear(&store, NOAH_LIGHTING_ARENA_CURRENT);
    assert(!arena.used);
    // All128 event IDs with independently addressable255 rules: common captured
    // histories encode membership, not a255x128 metadata reservation. Reference
    // wrap/compaction cannot alter rule/thing iteration or capture order.
    arena.next_reference = UINT32_MAX - 2u;
    for (unsigned thing = 0; thing < 128; thing++)
        for (unsigned rule = 0; rule < 255; rule++)
            event(rule, thing, 1000 + thing, thing % 60);
    assert(cohorts(NOAH_LIGHTING_ARENA_CURRENT) == 128);
    assert(arena.used == 128u * 80u);
    noah_lighting_history_cursor_t cursor;
    noah_lighting_history_store_cursor(&store, &cursor);
    unsigned logical = 0;
    while (noah_lighting_history_store_next(&store, NOAH_LIGHTING_ARENA_CURRENT, &cursor, &cell)) {
        assert(noah_lighting_retained_rule(&cell) == logical / 128u && noah_lighting_retained_thing(&cell) == logical % 128u);
        assert(noah_lighting_retained_order(&cell) == 1000 + logical % 128u);
        logical++;
    }
    assert(logical == 255u * 128u);
    assert(noah_lighting_history_store_freeze(&store));
    uint64_t scans = 0u;
    paints         = 0;
    last_entry     = 0;
    assert(noah_lighting_retained_render(&store, NOAH_LIGHTING_ARENA_FRAME, 254, &duration, captured, draw_all, NULL, &scans));
    assert(paints == 128u && last_entry == 1127u && scans == 128u * 129u);
    // Every current logical rule/thing can now be removed; frame membership and
    // full owner/slot/outcome payloads still render all128 subjects, beyond the
    // old64 placement scratch. The renderer retains no one-subject-array cap.
    noah_lighting_history_store_clear(&store, NOAH_LIGHTING_ARENA_CURRENT);
    paints     = 0;
    last_entry = 0;
    assert(noah_lighting_retained_render(&store, NOAH_LIGHTING_ARENA_FRAME, 254, &duration, captured, draw_all, NULL, NULL));
    assert(paints == 128u);
    printf("frozen streaming:128 subjects beyond64 scratch, candidate visits=%llu, frame-only bytes=%u; no target-time claim\n", (unsigned long long)scans, arena.used);
    noah_lighting_history_store_clear(&store, NOAH_LIGHTING_ARENA_FRAME);
    assert(!arena.used);
    printf("one-arena histories: singleton49B/full255 cohort80B/full128 shared occurrences%uB; current+frame first%uB; logical%u independent keys, split/rejoin/expiry/ACK/pins/failure/refwrap PASS (samples, NOT accepted-domain capacity proof)\n", 128u * 80u, frozen_peak, logical);
    noah_lighting_history_store_clear(&store, NOAH_LIGHTING_ARENA_CURRENT);
    assert(!arena.used);
}
