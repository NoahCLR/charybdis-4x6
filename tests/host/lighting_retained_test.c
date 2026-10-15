#include "users/noah/lib/rgb/lighting/lighting_retained.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t                     storage[4096]; // host workspace only; no target bank selected
static noah_lighting_retained_t    cells[255];
static noah_lighting_batch_arena_t arena;
static uint8_t                     batch[] = {1, 0x17, 3, 4, 0x80, 0, 0, 0, 0, 0, 0, 0x80};
static bool                        observe(noah_lighting_retained_t *cell, const noah_lighting_lifetime_parameters_t *p, uint32_t delta, uint8_t observation, uint64_t order, bool source) {
    return noah_lighting_retained_observe(cell, &arena, p, delta, observation, 4, batch, sizeof batch, order, source);
}
static void expect_payload(const noah_lighting_retained_t *cell, uint8_t slot, uint8_t outcome, uint8_t owner) {
    uint8_t actual[sizeof batch];
    assert(noah_lighting_batch_copy(&arena, noah_lighting_retained_batch(cell), 0, actual, sizeof actual));
    assert(actual[2] == slot && actual[3] == outcome && actual[4] == owner);
}
int main(void) {
    const uint8_t event = NOAH_LIGHTING_LIFETIME_EVENT, idle = NOAH_LIGHTING_LIFETIME_IDLE, nonqual = NOAH_LIGHTING_LIFETIME_NONQUALIFYING, match = NOAH_LIGHTING_LIFETIME_MATCH, depart = NOAH_LIGHTING_LIFETIME_DEPART;
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    noah_lighting_retained_t current, frozen;
    noah_lighting_retained_init(&current, 254, 127);
    noah_lighting_lifetime_parameters_t p = {.show_after = 200, .flash = 500};
    assert(!noah_lighting_retained_visible(&current, &p)); // no boot occurrence
    assert(observe(&current, &p, 0, event, 100, true));
    assert(noah_lighting_retained_dirty(&current) && !noah_lighting_retained_visible(&current, &p));
    assert(noah_lighting_retained_wake(&current, &p) == 200);
    assert(observe(&current, &p, 199, idle, 0, false) && !noah_lighting_retained_visible(&current, &p));
    assert(observe(&current, &p, 1, idle, 0, false) && noah_lighting_retained_visible(&current, &p));
    assert(noah_lighting_retained_pin(&current, &arena, &frozen));
    // Exact duplicate commit never restarts age; nonqualifying repeat changes
    // neither payload nor order, though local observation time still advances.
    assert(observe(&current, &p, 0, event, 100, true));
    assert(noah_lighting_retained_visible(&current, &p));
    batch[2] = 5;
    batch[3] = 6;
    batch[4] = 1;
    assert(observe(&current, &p, 10, nonqual, 101, true));
    expect_payload(&current, 3, 4, 0x80);
    assert(noah_lighting_retained_order(&current) == 100);
    assert(observe(&current, &p, 0, event, 102, true));
    expect_payload(&current, 5, 6, 1);
    assert(!noah_lighting_retained_visible(&current, &p));
    expect_payload(&frozen, 3, 4, 0x80);
    assert(noah_lighting_retained_visible(&frozen, &p));
    assert(!noah_lighting_retained_ack(&current, &arena, 100) && noah_lighting_retained_dirty(&current));
    assert(observe(&current, &p, 500, idle, 0, false)); // expires but unACKed survives
    assert(!noah_lighting_retained_visible(&current, &p) && noah_lighting_retained_dirty(&current));
    expect_payload(&current, 5, 6, 1);
    expect_payload(&frozen, 3, 4, 0x80);
    assert(noah_lighting_retained_ack(&current, &arena, 102));
    assert(!noah_lighting_retained_batch(&current));
    expect_payload(&frozen, 3, 4, 0x80);
    noah_lighting_retained_discard(&frozen, &arena);
    assert(arena.used == 0);
    // First receiver observation is its own clock even if the sender expired.
    noah_lighting_retained_init(&current, 254, 127);
    p.show_after = 0;
    assert(observe(&current, &p, 0, event, 100, false) && noah_lighting_retained_visible(&current, &p));
    assert(observe(&current, &p, 499, idle, 0, false) && noah_lighting_retained_visible(&current, &p));
    assert(observe(&current, &p, 1, idle, 0, false) && !noah_lighting_retained_batch(&current));
    // A failed reserve never advances clocks or changes current/dirty refs.
    assert(observe(&current, &p, 0, event, 103, true));
    noah_lighting_retained_t before = current;
    uint32_t                 used   = arena.used;
    arena.capacity                  = used;
    batch[3]++;
    assert(!observe(&current, &p, 99, event, 104, true));
    assert(!memcmp(&before, &current, sizeof current) && arena.used == used);
    arena.capacity = sizeof storage;
    assert(observe(&current, &p, 99, event, 104, true));
    noah_lighting_retained_discard(&current, &arena);
    assert(arena.used == 0);
    // State show-after cannot mature after departure; eligible whole batches
    // freeze, retaining departed owners/slots/outcomes across remaps.
    p = (noah_lighting_lifetime_parameters_t){.show_after = 100, .linger = 200};
    assert(observe(&current, &p, 0, match, 0, false));
    assert(!noah_lighting_retained_batch(&current));
    assert(observe(&current, &p, 50, depart, 0, false));
    assert(!noah_lighting_retained_visible(&current, &p));
    assert(observe(&current, &p, 0, match, 0, false));
    assert(observe(&current, &p, 100, match, 0, false) && noah_lighting_retained_visible(&current, &p));
    batch[4] = 3;
    assert(observe(&current, &p, 49, match, 0, false)); // partial departure updates whole current batch
    assert(observe(&current, &p, 1, depart, 0, false));
    expect_payload(&current, 5, 7, 3);
    batch[2] = 12;
    batch[3] = 99;
    batch[4] = 0x40; // new mapping/producer scratch
    assert(observe(&current, &p, 199, idle, 0, false));
    expect_payload(&current, 5, 7, 3);
    assert(observe(&current, &p, 1, idle, 0, false) && !noah_lighting_retained_batch(&current));
    assert(arena.used == 0);
    // Separate255 filters/ages/version bits share identical payload bytes only.
    p = (noah_lighting_lifetime_parameters_t){.flash = 65535};
    for (unsigned r = 0; r < 255; r++) {
        noah_lighting_retained_init(&cells[r], r, 127);
        assert(observe(&cells[r], &p, 0, event, 100 + r, true));
    }
    assert(arena.used == NOAH_LIGHTING_BATCH_ENTRY_HEADER + sizeof batch);
    assert(observe(&cells[0], &p, 65535, idle, 0, false));
    assert(!noah_lighting_retained_visible(&cells[0], &p));
    assert(noah_lighting_retained_dirty(&cells[0]));
    assert(noah_lighting_retained_visible(&cells[254], &p) && noah_lighting_retained_dirty(&cells[254]));
    assert(noah_lighting_retained_ack(&cells[0], &arena, 100));
    assert(!noah_lighting_retained_batch(&cells[0]));
    for (unsigned r = 1; r < 255; r++)
        noah_lighting_retained_discard(&cells[r], &arena);
    assert(arena.used == 0);
    puts("retained ownership:18B independent metadata, exact show/flash/linger boundaries, D1/D2, pending expiry/ACK versions, frozen render refs, atomic reserve, remap and255 independent filters PASS (no target capacity claim)");
}
