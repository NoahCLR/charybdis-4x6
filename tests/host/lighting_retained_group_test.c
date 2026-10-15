#include "users/noah/lib/rgb/lighting/lighting_retained_group.h"
#include "users/noah/lib/rgb/lighting/lighting_owned_batch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static noah_lighting_retained_t cells[255], saved[255];
static uint8_t                  storage[2048], encoded[NOAH_LIGHTING_GROUP_BATCH_BYTES];
static bool                     allow_prepare = true, allow_records = true;
static unsigned                 lookups, prepares;
static bool                     prepare(void *context, const noah_lighting_event_group_t *group) {
    (void)context;
    assert(group->thing == 127);
    prepares++;
    return allow_prepare;
}
static noah_lighting_retained_t *record(void *context, uint8_t rule, uint8_t thing, noah_lighting_lifetime_parameters_t *parameters) {
    (void)context;
    assert(thing == 127);
    lookups++;
    *parameters = (noah_lighting_lifetime_parameters_t){.show_after = rule % 2 ? 700 : 200, .flash = 500};
    return allow_records ? &cells[rule] : NULL;
}
int main(void) {
    noah_lighting_batch_arena_t arena;
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    for (unsigned r = 0; r < 255; r++)
        noah_lighting_retained_init(&cells[r], r, 127);
    noah_lighting_event_group_t group = {.order = 100, .thing_kind = 4, .thing = 127, .slot = 3, .outcome = 7, .sides = 3, .has_keys = 1};
    memset(group.owners, 0xff, sizeof group.owners);
    memset(group.qualified, 0xff, sizeof group.qualified);
    group.qualified[31]                  = 0x7f;
    noah_lighting_event_group_t original = group;
    memcpy(saved, cells, sizeof saved);
    allow_prepare = false;
    assert(!noah_lighting_retained_group_apply(&arena, &group, true, encoded, prepare, record, NULL));
    assert(!arena.used && !memcmp(saved, cells, sizeof saved));
    allow_prepare = true;
    allow_records = false;
    assert(!noah_lighting_retained_group_apply(&arena, &group, true, encoded, prepare, record, NULL));
    assert(!arena.used && !memcmp(saved, cells, sizeof saved));
    allow_records  = true;
    arena.capacity = 0;
    assert(!noah_lighting_retained_group_apply(&arena, &group, true, encoded, prepare, record, NULL));
    assert(!arena.used && !memcmp(saved, cells, sizeof saved));
    arena.capacity = sizeof storage;
    lookups        = 0;
    assert(noah_lighting_retained_group_apply(&arena, &group, true, encoded, prepare, record, NULL));
    assert(lookups == 510);
    assert(!memcmp(&group, &original, sizeof group));
    uint32_t refs;
    assert(noah_lighting_batch_describe(&arena, noah_lighting_retained_batch(&cells[0]), NULL, NULL, NULL, &refs) && refs == 255);
    assert(arena.used == NOAH_LIGHTING_BATCH_ENTRY_HEADER + NOAH_LIGHTING_GROUP_BATCH_BYTES);
    noah_lighting_lifetime_parameters_t p = {.show_after = 200, .flash = 500};
    assert(noah_lighting_retained_observe(&cells[0], &arena, &p, 200, NOAH_LIGHTING_LIFETIME_IDLE, 4, NULL, 0, 0, false));
    assert(noah_lighting_retained_visible(&cells[0], &p));
    assert(noah_lighting_retained_group_apply(&arena, &group, true, encoded, prepare, record, NULL)); // duplicate no restart
    assert(noah_lighting_retained_visible(&cells[0], &p));
    // Markers waiting inside show-after also seed a new session, using captured
    // qualification rather than today's filters, without restarting source age.
    assert(noah_lighting_retained_ack(&cells[254], &arena, 100));
    noah_lighting_retained_seed(&cells[254], &arena);
    assert(noah_lighting_retained_dirty(&cells[254]));
    noah_lighting_retained_t frozen;
    assert(noah_lighting_retained_pin(&cells[254], &arena, &frozen));
    group.order     = 102;
    group.slot      = 1;
    group.owners[0] = 1;
    memset(group.qualified, 0, sizeof group.qualified);
    group.qualified[0]  = 1;
    group.qualified[31] = 0x40;
    assert(noah_lighting_retained_group_apply(&arena, &group, true, encoded, prepare, record, NULL));
    assert(noah_lighting_retained_order(&cells[0]) == 102 && noah_lighting_retained_order(&cells[254]) == 102);
    assert(noah_lighting_retained_order(&cells[1]) == 100); // nonqualifying repeat preserves older
    assert(!noah_lighting_retained_ack(&cells[254], &arena, 100) && noah_lighting_retained_dirty(&cells[254]));
    noah_lighting_owned_batch_reader_t reader;
    noah_lighting_subject_t            subject;
    assert(noah_lighting_owned_batch_begin(&arena, noah_lighting_retained_batch(&frozen), &reader));
    assert(noah_lighting_owned_batch_next(&arena, &reader, &subject));
    assert(subject.slot == 3 && subject.outcome == 7 && subject.keys[0] == 255);
    assert(noah_lighting_retained_observe(&cells[254], &arena, &p, 500, NOAH_LIGHTING_LIFETIME_IDLE, 4, NULL, 0, 0, false));
    assert(!noah_lighting_retained_visible(&cells[254], &p) && noah_lighting_retained_dirty(&cells[254]));
    noah_lighting_retained_seed(&cells[254], &arena);
    assert(!noah_lighting_retained_batch(&cells[254])); // expired cannot replay fresh
    for (unsigned r = 0; r < 255; r++)
        noah_lighting_retained_discard(&cells[r], &arena);
    noah_lighting_retained_discard(&frozen, &arena);
    assert(arena.used == 0);
    puts("qualified group retention:255 independent cells/one full batch, all-or-none reserve, duplicate age, D2 subset repeats, frozen owners, newer ACK isolation, live waiting seed/expired discard PASS");
}
