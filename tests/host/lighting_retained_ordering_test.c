#include "users/noah/lib/rgb/lighting/lighting_retained.h"
#include "users/noah/lib/rgb/lighting/lighting_owned_batch.h"
#include "users/noah/lib/rgb/lighting/lighting_priority.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// Materialized fact batches test the lifetime/owned-batch/merge seam. These
// small host fixtures are neither a production bank nor a reachability proof.
typedef struct {
    noah_lighting_retained_t            record;
    noah_lighting_lifetime_parameters_t duration;
    uint8_t                             kind, thing, natural;
    char                                colour[8];
    bool                                matching;
} cell_t;
typedef struct {
    noah_lighting_priority_t priority;
    unsigned                 cell;
} placed_t;
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *input = fopen(argv[1], "r");
    assert(input);
    cell_t                      cells[4] = {0};
    unsigned                    count = 0, cases = 0, steps = 0;
    uint8_t                     storage[4096], encoded[NOAH_LIGHTING_BATCH_BYTES];
    noah_lighting_batch_arena_t arena;
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    char word[100], name[100];
    while (fscanf(input, "%99s", word) == 1) {
        if (!strcmp(word, "CASE")) {
            for (unsigned c = 0; c < count; c++)
                noah_lighting_retained_discard(&cells[c].record, &arena);
            assert(!arena.used);
            memset(cells, 0, sizeof cells);
            assert(fscanf(input, "%99s %u", name, &count) == 2 && count <= 4);
            for (unsigned c = 0; c < count; c++) {
                unsigned kind, thing, natural, show, linger;
                assert(fscanf(input, "%u %u %u %u %u %7s", &kind, &thing, &natural, &show, &linger, cells[c].colour) == 6);
                cells[c].kind     = kind;
                cells[c].thing    = thing;
                cells[c].natural  = natural;
                cells[c].duration = (noah_lighting_lifetime_parameters_t){.show_after = show, .linger = linger};
                noah_lighting_retained_init(&cells[c].record, 254, thing);
            }
            cases++;
            continue;
        }
        assert(!strcmp(word, "STEP"));
        uint32_t elapsed;
        uint64_t order;
        assert(fscanf(input, "%" SCNu32 " %" SCNu64, &elapsed, &order) == 2);
        for (unsigned c = 0; c < count; c++) {
            unsigned members;
            assert(fscanf(input, "%u", &members) == 1 && members <= 64);
            noah_lighting_batch_writer_t writer;
            noah_lighting_batch_write_begin(&writer, encoded, sizeof encoded);
            for (unsigned m = 0; m < members; m++) {
                unsigned key;
                assert(fscanf(input, "%u", &key) == 1 && key < MATRIX_ROWS * MATRIX_COLS);
                noah_lighting_subject_t subject;
                noah_lighting_subject_init(&subject, cells[c].kind, cells[c].thing, m + 1);
                subject.has_keys = true;
                subject.keys[key / 8] |= 1u << (key % 8);
                assert(noah_lighting_batch_write(&writer, &subject));
            }
            unsigned observation = members ? NOAH_LIGHTING_LIFETIME_MATCH : cells[c].matching ? NOAH_LIGHTING_LIFETIME_DEPART : NOAH_LIGHTING_LIFETIME_IDLE;
            assert(noah_lighting_retained_observe(&cells[c].record, &arena, &cells[c].duration, elapsed, observation, cells[c].kind, encoded, writer.used, order, false));
            cells[c].matching = !!members;
        }
        placed_t placed[256];
        unsigned n = 0;
        for (unsigned c = 0; c < count; c++)
            if (noah_lighting_retained_visible(&cells[c].record, &cells[c].duration)) {
                noah_lighting_owned_batch_reader_t reader;
                noah_lighting_subject_t            subject;
                assert(noah_lighting_owned_batch_begin(&arena, noah_lighting_retained_batch(&cells[c].record), &reader));
                unsigned ordinal = 0;
                while (noah_lighting_owned_batch_next(&arena, &reader, &subject)) {
                    unsigned key = 0;
                    while (key < MATRIX_ROWS * MATRIX_COLS && !(subject.keys[key / 8] & (1u << (key % 8))))
                        key++;
                    uint64_t natural = cells[c].natural == 0 ? key : cells[c].natural == 1 ? 255u - subject.thing : 0;
                    placed_t value   = {.priority = {natural, noah_lighting_retained_order(&cells[c].record), subject.thing_kind, subject.thing, ordinal++}, .cell = c};
                    unsigned at      = n++;
                    while (at && noah_lighting_priority_before(&value.priority, &placed[at - 1].priority)) {
                        placed[at] = placed[at - 1];
                        at--;
                    }
                    placed[at] = value;
                }
            }
        char     pixel[8];
        unsigned expected_n;
        assert(fscanf(input, "%7s %u", pixel, &expected_n) == 2);
        const char *actual = n ? cells[placed[0].cell].colour : "-";
        if (strcmp(pixel, actual) || expected_n != n) {
            fprintf(stderr, "%s step%u expected%s/%u got%s/%u\n", name, steps, pixel, expected_n, actual, n);
            return 1;
        }
        for (unsigned m = 0; m < n; m++) {
            unsigned cell, ordinal;
            assert(fscanf(input, "%u %u", &cell, &ordinal) == 2);
            assert(placed[m].cell == cell && placed[m].priority.ordinal == ordinal);
        }
        for (unsigned c = 0; c < count; c++) {
            uint64_t expected;
            assert(fscanf(input, "%" SCNu64, &expected) == 1);
            assert(noah_lighting_retained_order(&cells[c].record) == expected);
        }
        steps++;
    }
    fclose(input);
    for (unsigned c = 0; c < count; c++)
        noah_lighting_retained_discard(&cells[c].record, &arena);
    assert(!arena.used);
    printf("retained priority: %u shared pixel/order traces, %u steps, natural priority before entry recency; refresh/reentry/frozen-underlay/timer boundary PASS\n", cases, steps);
}
