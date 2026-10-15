#include "users/noah/lib/rgb/lighting/lighting_history_bank.h"
#include "users/noah/lib/rgb/lighting/lighting_owned_batch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static noah_profile_lighting_v1_program_t program;
static noah_lighting_history_index_t      index_table;
static noah_lighting_history_bank_t       bank;
static noah_lighting_batch_arena_t        arena;
static uint8_t                            storage[256];
static void                               begin(unsigned show, unsigned linger, unsigned flash) {
    if (bank.program) noah_lighting_history_store_clear(&bank.store, NOAH_LIGHTING_ARENA_CURRENT);
    assert(!arena.used);
    uint8_t        bytes[40] = {5, 0, 58, 0, 0, 0, 0, 1, 0, 0, 255, 1, flash ? 28 : 14, flash ? 127 : 59, 0x40, 4, 0x60, 0};
    unsigned       used      = 18;
    const unsigned values[]  = {show, linger, flash};
    for (unsigned i = 0; i < 3; i++)
        if (values[i]) {
            bytes[used++] = 0xa2;
            bytes[used++] = i + 1;
            bytes[used++] = values[i];
            bytes[used++] = values[i] >> 8;
        }
    bytes[8]                                 = used - 9;
    bytes[used++]                            = 0;
    noah_profile_lighting_v1_limits_t limits = noah_profile_lighting_v1_default_limits();
    noah_profile_lighting_v1_view_t   view;
    assert(noah_profile_lighting_v1_decode(bytes, used, &limits, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view, &program));
    assert(noah_lighting_history_index_prepare(&program, &index_table));
    assert(index_table.offsets[1] == 1);
    noah_lighting_history_bank_begin(&bank, &program, &arena);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *input = fopen(argv[1], "r");
    assert(input);
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    unsigned cases = 0, steps = 0, show = 0, linger = 0, flash = 0;
    char     word[96], name[96];
    while (fscanf(input, "%95s", word) == 1) {
        if (!strcmp(word, "CASE")) {
            assert(fscanf(input, "%95s %u %u %u", name, &show, &linger, &flash) == 4);
            begin(show, linger, flash);
            cases++;
            continue;
        }
        unsigned long delta;
        unsigned      operation, marker, visible, wake, expected_marker;
        assert(!strcmp(word, "STEP") && fscanf(input, "%lu %u %u %u %u %u", &delta, &operation, &marker, &visible, &wake, &expected_marker) == 6);
        assert(noah_lighting_history_bank_advance(&bank, (uint32_t)delta));
        uint8_t thing = flash ? 127 : 59;
        if (operation == NOAH_LIGHTING_LIFETIME_EVENT) {
            noah_lighting_event_group_t event = {.order = steps + 1u, .thing_kind = 5, .thing = 127, .slot = marker, .has_keys = 1, .sides = 2};
            event.owners[7]                   = 8;
            event.qualified[0]                = 1;
            uint8_t scratch[(1u + 3u + KEY_ORIGIN_BITMAP_SIZE)];
            assert(noah_lighting_history_bank_event(&bank, &event, false, scratch));
        } else if (operation == NOAH_LIGHTING_LIFETIME_MATCH) {
            uint8_t                 batch[(1u + 3u + KEY_ORIGIN_BITMAP_SIZE)];
            noah_lighting_subject_t subject;
            noah_lighting_subject_init(&subject, 7, 59, marker);
            subject.has_keys = true;
            subject.sides    = 2;
            subject.keys[7]  = 8;
            noah_lighting_batch_writer_t writer;
            noah_lighting_batch_write_begin(&writer, batch, sizeof batch);
            assert(noah_lighting_batch_write(&writer, &subject));
            assert(noah_lighting_history_bank_state(&bank, 0, 59, operation, batch, writer.used, steps + 1u));
        } else if (operation == NOAH_LIGHTING_LIFETIME_DEPART)
            assert(noah_lighting_history_bank_state(&bank, 0, 59, operation, NULL, 0, steps + 1u));
        noah_lighting_lifetime_parameters_t parameters = {show, linger, flash};
        noah_lighting_retained_t            copy;
        const noah_lighting_retained_t     *cell   = noah_lighting_history_store_read(&bank.store, NOAH_LIGHTING_ARENA_CURRENT, 0, thing, &copy) ? &copy : NULL;
        bool                                actual = cell && noah_lighting_retained_visible(cell, &parameters);
        if (actual != !!visible || noah_lighting_history_bank_wake(&bank) != wake) {
            fprintf(stderr, "profile owner %s step%u: expected visible%u wake%u got%u wake%lu\n", name, steps, visible, wake, actual, (unsigned long)noah_lighting_history_bank_wake(&bank));
            return 1;
        }
        noah_lighting_batch_ref_t reference = cell ? noah_lighting_retained_batch(cell) : 0;
        if (expected_marker) {
            noah_lighting_owned_batch_reader_t reader;
            noah_lighting_subject_t            subject;
            assert(noah_lighting_owned_batch_begin(&arena, reference, &reader));
            assert(noah_lighting_owned_batch_next(&arena, &reader, &subject) && subject.slot == expected_marker && !reader.remaining);
        } else
            assert(!reference);
        steps++;
    }
    fclose(input);
    noah_lighting_history_store_clear(&bank.store, NOAH_LIGHTING_ARENA_CURRENT);
    assert(!arena.used);
    printf("profile-owned shared lifetimes: %u cases/%u steps, actual validator/timing modules/index/state+event owner/show/linger/reentry/expiry/wakes/maxdelay PASS\n", cases, steps);
}
