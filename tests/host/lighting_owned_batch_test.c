#include "users/noah/lib/rgb/lighting/lighting_owned_batch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t                 arena_bytes[4u * NOAH_LIGHTING_BATCH_BYTES];
static uint8_t                 encoded[NOAH_LIGHTING_BATCH_BYTES];
static noah_lighting_subject_t expected[64];
int                            main(void) {
    noah_lighting_batch_arena_t arena;
    noah_lighting_batch_arena_init(&arena, arena_bytes, sizeof arena_bytes);
    noah_lighting_batch_ref_t decoy = 0, old = 0, newer = 0;
    uint8_t                   empty = 0;
    assert(noah_lighting_batch_acquire(&arena, 0, 255, &empty, 1, &decoy, NULL));
    noah_lighting_batch_writer_t writer;
    noah_lighting_batch_write_begin(&writer, encoded, sizeof encoded);
    for (unsigned i = 0; i < 64; i++) {
        noah_lighting_subject_init(&expected[i], 4, 127, i + 1);
        expected[i].outcome  = i + 2;
        expected[i].has_keys = true;
        expected[i].sides    = 3;
        memset(expected[i].keys, 0xff, sizeof expected[i].keys);
        assert(noah_lighting_batch_write(&writer, &expected[i]));
    }
    assert(writer.used == NOAH_LIGHTING_BATCH_BYTES);
    assert(noah_lighting_batch_acquire(&arena, 4, 127, encoded, writer.used, &old, NULL));
    assert(noah_lighting_batch_retain(&arena, old)); // frozen renderer
    noah_lighting_owned_batch_reader_t reader;
    assert(noah_lighting_owned_batch_begin(&arena, old, &reader));
    noah_lighting_subject_t subject;
    assert(noah_lighting_owned_batch_next(&arena, &reader, &subject));
    assert(!memcmp(&subject, &expected[0], sizeof subject));
    assert(noah_lighting_batch_release(&arena, decoy)); // moves the active reader's payload
    // New occurrence/remap updates current ownership, not the pinned renderer.
    encoded[writer.used - 1u] ^= 0x80;
    assert(noah_lighting_batch_acquire(&arena, 4, 127, encoded, writer.used, &newer, NULL));
    assert(newer != old);
    assert(noah_lighting_batch_release(&arena, old));
    memset(encoded, 0, sizeof encoded); // encoder workspace already reusable
    for (unsigned i = 1; i < 64; i++) {
        assert(noah_lighting_owned_batch_next(&arena, &reader, &subject));
        assert(!memcmp(&subject, &expected[i], sizeof subject));
    }
    assert(!reader.remaining && reader.offset == reader.size && !noah_lighting_owned_batch_next(&arena, &reader, &subject));
    assert(noah_lighting_batch_release(&arena, old));
    assert(noah_lighting_owned_batch_begin(&arena, newer, &reader));
    for (unsigned i = 0; i < 64; i++) {
        assert(noah_lighting_owned_batch_next(&arena, &reader, &subject));
        if (i == 63) expected[i].keys[KEY_ORIGIN_BITMAP_SIZE - 1u] ^= 0x80;
        assert(!memcmp(&subject, &expected[i], sizeof subject));
    }
    assert(noah_lighting_batch_release(&arena, newer) && !arena.used);
    // Common paint fields are inherited across members, while duplicate
    // members/order are preserved rather than collapsed to a final LED mask.
    noah_lighting_batch_write_begin(&writer, encoded, sizeof encoded);
    for (unsigned i = 0; i < 64; i++) {
        expected[i] = expected[0];
        assert(noah_lighting_batch_write(&writer, &expected[i]));
    }
    assert(noah_lighting_batch_acquire(&arena, 4, 127, encoded, writer.used, &old, NULL));
    assert(noah_lighting_owned_batch_begin(&arena, old, &reader));
    for (unsigned i = 0; i < 64; i++) {
        assert(noah_lighting_owned_batch_next(&arena, &reader, &subject));
        assert(!memcmp(&subject, &expected[i], sizeof subject));
    }
    assert(reader.offset == reader.size);
    assert(noah_lighting_batch_release(&arena, old));
    // Mutable current metadata and immutable frame metadata live in this same
    // arena, but can never intern with an immutable batch or each other.
    uint8_t meta[] = {0xab, 0xcd, 0xef}, changed[64];
    memset(changed, 0x42, sizeof changed);
    noah_lighting_batch_ref_t c1, c2, f1, b1, b2;
    assert(noah_lighting_record_acquire(&arena, NOAH_LIGHTING_ARENA_CURRENT, 4, 127, meta, sizeof meta, &c1));
    assert(noah_lighting_record_acquire(&arena, NOAH_LIGHTING_ARENA_CURRENT, 4, 127, meta, sizeof meta, &c2));
    assert(c1 != c2);
    assert(noah_lighting_batch_acquire(&arena, 4, 127, meta, sizeof meta, &b1, NULL));
    assert(noah_lighting_batch_acquire(&arena, 4, 127, meta, sizeof meta, &b2, NULL));
    assert(b1 == b2 && b1 != c1 && b1 != c2);
    assert(noah_lighting_record_acquire(&arena, NOAH_LIGHTING_ARENA_FRAME, 4, 127, meta, sizeof meta, &f1));
    uint8_t type_value;
    assert(noah_lighting_record_type(&arena, c1, &type_value) && type_value == NOAH_LIGHTING_ARENA_CURRENT);
    assert(!noah_lighting_owned_batch_begin(&arena, c1, &reader));
    assert(noah_lighting_batch_retain(&arena, c1));
    assert(!noah_lighting_record_update(&arena, c1, changed, sizeof changed)); // shared current cannot mutate
    assert(noah_lighting_batch_release(&arena, c1));
    assert(noah_lighting_record_update(&arena, c1, changed, sizeof changed));
    assert(!noah_lighting_record_update(&arena, b1, changed, sizeof changed));
    assert(!noah_lighting_record_update(&arena, f1, changed, sizeof changed));
    noah_lighting_batch_ref_t previous = c1;
    assert(!noah_lighting_batch_replace(&arena, &c1, 4, 127, meta, sizeof meta, NULL) && c1 == previous);
    uint8_t copied[3];
    assert(noah_lighting_batch_copy(&arena, b1, 0, copied, sizeof copied) && !memcmp(copied, meta, sizeof meta));
    assert(noah_lighting_batch_copy(&arena, f1, 0, copied, sizeof copied) && !memcmp(copied, meta, sizeof meta));
    uint32_t                  offset = 0;
    noah_lighting_batch_ref_t next;
    assert(noah_lighting_record_next(&arena, NOAH_LIGHTING_ARENA_CURRENT, &offset, &next) && next == c1);
    assert(noah_lighting_record_next(&arena, NOAH_LIGHTING_ARENA_CURRENT, &offset, &next) && next == c2);
    assert(!noah_lighting_record_next(&arena, NOAH_LIGHTING_ARENA_CURRENT, &offset, &next));
    assert(noah_lighting_batch_release(&arena, c2));
    assert(noah_lighting_record_update(&arena, c1, changed, 1));
    uint32_t capacity = arena.capacity;
    arena.capacity    = arena.used;
    assert(!noah_lighting_record_update(&arena, c1, changed, sizeof changed));
    arena.capacity = capacity;
    assert(noah_lighting_batch_copy(&arena, c1, 0, copied, 1) && copied[0] == 0x42);
    assert(noah_lighting_batch_copy(&arena, f1, 0, copied, sizeof copied) && !memcmp(copied, meta, sizeof meta));
    assert(noah_lighting_batch_release(&arena, b1) && noah_lighting_batch_release(&arena, b2));
    assert(noah_lighting_batch_release(&arena, c1) && noah_lighting_batch_release(&arena, f1) && !arena.used);
    printf("owned ordered iterator %ux%u: max%uB payload, bounded one-member copy, compaction mid-read, pinned old/new owners, slot/outcome/order/multiplicity PASS\n", MATRIX_ROWS, MATRIX_COLS, (unsigned)NOAH_LIGHTING_BATCH_BYTES);
}
