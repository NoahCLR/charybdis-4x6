#include "users/noah/lib/rgb/lighting/lighting_batch_arena.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { RULES = 255u, THINGS = 128u, BODY = 10u, ENTRIES = RULES * THINGS, CAPACITY = ENTRIES * (NOAH_LIGHTING_BATCH_ENTRY_HEADER + BODY) };
// Host-only adversarial storage; NOT a firmware reservation or fit proof.
static uint8_t                   storage[CAPACITY], before[CAPACITY];
static noah_lighting_batch_ref_t references[RULES][THINGS];
static void                      body(uint8_t bytes[BODY], uint8_t rule) {
    memset(bytes, 0, BODY);
    bytes[0] = 1;
    bytes[1] = 0x35;
    bytes[2] = rule;
    bytes[9] = 0x80;
}
static void expect(noah_lighting_batch_arena_t *arena, noah_lighting_batch_ref_t reference, uint8_t thing, uint8_t rule, uint32_t count) {
    uint8_t  kind_out, thing_out, actual[BODY], expected[BODY];
    uint16_t size;
    uint32_t refs;
    assert(noah_lighting_batch_describe(arena, reference, &kind_out, &thing_out, &size, &refs));
    assert(kind_out == 4 && thing_out == thing && size == BODY && refs == count);
    body(expected, rule);
    assert(noah_lighting_batch_copy(arena, reference, 0, actual, BODY));
    assert(!memcmp(actual, expected, BODY));
}
int main(void) {
    noah_lighting_batch_arena_t arena;
    uint8_t                     bytes[BODY];
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    // 255 independent rules share only the identical payload, not clocks or
    // filter qualification. Each can retain/depart/expire independently.
    for (unsigned r = 0; r < RULES; r++)
        for (unsigned t = 0; t < THINGS; t++) {
            body(bytes, 0);
            assert(noah_lighting_batch_acquire(&arena, 4, (uint8_t)t, bytes, BODY, &references[r][t], NULL));
        }
    assert(arena.used == THINGS * (NOAH_LIGHTING_BATCH_ENTRY_HEADER + BODY));
    for (unsigned t = 0; t < THINGS; t++)
        expect(&arena, references[0][t], t, 0, RULES);
    // Freeze old render and unACKed delivery versions. A newer qualifying
    // repeat owns a new reference; ACKing/releasing old never touches it.
    noah_lighting_batch_ref_t frozen = references[0][127];
    assert(noah_lighting_batch_retain(&arena, frozen));
    assert(noah_lighting_batch_retain(&arena, frozen));
    body(bytes, 254);
    noah_lighting_batch_ref_t newer = 0;
    assert(noah_lighting_batch_acquire(&arena, 4, 127, bytes, BODY, &newer, NULL));
    assert(newer != frozen);
    assert(noah_lighting_batch_release(&arena, references[0][127]));
    references[0][127] = newer;
    expect(&arena, newer, 127, 254, 1);
    expect(&arena, frozen, 127, 0, RULES + 1);
    assert(noah_lighting_batch_release(&arena, frozen));
    expect(&arena, newer, 127, 254, 1);
    // Remove preceding entries/compact while the old render keeps its ref.
    for (unsigned r = 0; r < RULES; r++)
        for (unsigned t = 0; t < 127; t++)
            assert(noah_lighting_batch_release(&arena, references[r][t]));
    expect(&arena, frozen, 127, 0, RULES);
    expect(&arena, newer, 127, 254, 1);
    for (unsigned r = 1; r < RULES; r++)
        assert(noah_lighting_batch_release(&arena, references[r][127]));
    expect(&arena, frozen, 127, 0, 1);
    assert(noah_lighting_batch_release(&arena, frozen));
    expect(&arena, newer, 127, 254, 1);
    assert(noah_lighting_batch_release(&arena, newer));
    assert(arena.used == 0);
    // A sole owner can replace while another handle pins the old payload.
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    body(bytes, 1);
    noah_lighting_batch_ref_t owner = 0;
    assert(noah_lighting_batch_replace(&arena, &owner, 4, 127, bytes, BODY, NULL));
    frozen = owner;
    assert(noah_lighting_batch_retain(&arena, frozen));
    body(bytes, 2);
    assert(noah_lighting_batch_replace(&arena, &owner, 4, 127, bytes, BODY, NULL));
    expect(&arena, frozen, 127, 1, 1);
    expect(&arena, owner, 127, 2, 1);
    assert(noah_lighting_batch_replace(&arena, &owner, 4, 127, bytes, BODY, NULL));
    expect(&arena, owner, 127, 2, 1); // equal replacement balances its reference
    assert(noah_lighting_batch_release(&arena, frozen));
    assert(noah_lighting_batch_release(&arena, owner));
    // All accepted rule/id indices and collision-heavy equal prefixes, no
    // hash-based merging. This tests representation, not physical reachability.
    noah_lighting_batch_search_t cost = {0};
    for (unsigned r = 0; r < RULES; r++)
        for (unsigned t = 0; t < THINGS; t++) {
            body(bytes, r);
            assert(noah_lighting_batch_acquire(&arena, 4, t, bytes, BODY, &references[r][t], &cost));
        }
    assert(arena.used == CAPACITY);
    for (unsigned r = 0; r < RULES; r++)
        expect(&arena, references[r][127], 127, r, 1);
    // Reserve-before-release failure is atomic even at old+new overlap peak.
    memcpy(before, storage, sizeof storage);
    uint32_t                  used = arena.used, next = arena.next_reference;
    noah_lighting_batch_ref_t unchanged = 1234;
    body(bytes, 255);
    assert(!noah_lighting_batch_acquire(&arena, 4, 127, bytes, BODY, &unchanged, NULL));
    assert(unchanged == 1234 && arena.used == used && arena.next_reference == next && !memcmp(before, storage, sizeof storage));
    // A same-size old-key remap cannot change the frozen batch. Removing one
    // unrelated old entry creates exactly one new slot, without fragmentation.
    assert(noah_lighting_batch_release(&arena, references[0][0]));
    assert(noah_lighting_batch_acquire(&arena, 4, 127, bytes, BODY, &newer, NULL));
    expect(&arena, newer, 127, 255, 1);
    expect(&arena, references[254][127], 127, 254, 1);
    printf("arena adversary: %u independent rule/thing payloads, %u bytes = entries*(12+10); search entries=%u full-equality bytes=%u (not target-bank/CPU-cadence proof)\n", ENTRIES, CAPACITY, cost.entries, cost.equality_bytes);
    // Distinct full ordered maximum batches are never merged because they
    // paint the same final mask. Order/slot/outcome/owners all remain bytes.
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    uint8_t large[705], copied[705];
    memset(large, 0xff, sizeof large);
    large[0] = 64;
    for (unsigned i = 0; i < 64; i++) {
        large[1 + i * 11] = 0x17;
        large[2 + i * 11] = (uint8_t)(i + 1);
        large[3 + i * 11] = (uint8_t)(i + 2);
    }
    assert(noah_lighting_batch_acquire(&arena, 4, 127, large, sizeof large, &frozen, NULL));
    assert(noah_lighting_batch_retain(&arena, frozen));
    large[704] = 0x7f;
    assert(noah_lighting_batch_acquire(&arena, 4, 127, large, sizeof large, &newer, NULL) && newer != frozen);
    assert(arena.used == 2 * (NOAH_LIGHTING_BATCH_ENTRY_HEADER + 705));
    assert(noah_lighting_batch_release(&arena, frozen));
    assert(noah_lighting_batch_copy(&arena, frozen, 0, copied, sizeof copied) && copied[704] == 0xff);
    assert(noah_lighting_batch_release(&arena, frozen));
    assert(noah_lighting_batch_copy(&arena, newer, 0, copied, sizeof copied) && copied[704] == 0x7f);
    // Reset invalidates all previous histories; ID wrap skips zero/live IDs.
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    body(bytes, 0);
    noah_lighting_batch_ref_t first;
    assert(noah_lighting_batch_acquire(&arena, 4, 0, bytes, BODY, &first, NULL) && first == 1);
    arena.next_reference = UINT32_MAX;
    body(bytes, 1);
    assert(noah_lighting_batch_acquire(&arena, 4, 0, bytes, BODY, &newer, NULL) && newer == UINT32_MAX);
    body(bytes, 2);
    assert(noah_lighting_batch_acquire(&arena, 4, 0, bytes, BODY, &newer, NULL) && newer == 2);
    uint8_t value;
    assert(!noah_lighting_batch_copy(&arena, 0, 0, &value, 1));
    assert(!noah_lighting_batch_copy(&arena, first, BODY, &value, 1));
    assert(!noah_lighting_batch_release(&arena, 0));
    assert(!noah_lighting_batch_retain(&arena, 0));
    assert(noah_lighting_batch_describe(&arena, first, NULL, NULL, NULL, NULL));
    puts("owned batch arena: full equality, independent references, old+new/render/delivery overlap, compaction, atomic failure, high IDs and wrap PASS");
}
