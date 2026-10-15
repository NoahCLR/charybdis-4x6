#include "users/noah/lib/rgb/lighting/lighting_history_observe.h"
#include "users/noah/lib/rgb/lighting/lighting_owned_batch.h"
#include "matrix.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// Coherent frame fixtures drive the actual optional fact walkers. These
// collectors are deliberately unused: producer/collector integration has its
// own runner, while this seam owns observation of a completed immutable frame.
layer_state_t layer_state;
matrix_row_t  matrix_get_row(uint8_t row) {
    (void)row;
    return 0;
}
void key_feedback_lighting_map(uint8_t *bytes) {
    (void)bytes;
    assert(!"no recollection during observation");
}
bool noah_lighting_fact_next(uint8_t kind, const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    switch (kind) {
        case 1:
            return noah_lighting_fact_layer_active_next(facts, params, cursor, subject);
        case 8:
            return noah_lighting_fact_behaviour_next(facts, params, cursor, subject);
        case 14:
            return noah_lighting_fact_key_pressed_next(facts, params, cursor, subject);
        default:
            assert(!"unsupported fixture fact");
            return false;
    }
}
static noah_profile_lighting_v1_program_t program;
static noah_lighting_history_bank_t       bank;
static noah_lighting_batch_arena_t        arena;
static uint8_t                            storage[32768], scratch[NOAH_LIGHTING_BATCH_BYTES];
static void                               readhex(FILE *file, uint8_t *bytes, unsigned size) {
    for (unsigned i = 0; i < size; i++) {
        unsigned value;
        assert(fscanf(file, "%2x", &value) == 1);
        bytes[i] = value;
    }
}
static void full_rule_domain(void) {
    assert(program.rule_count == 255u);
    uint8_t bytes[8192];
    memcpy(bytes, program.bytes, program.length);
    for (unsigned rule = 0; rule < program.rule_count; rule++)
        bytes[program.rules[rule] + 1u] = 0u;
    noah_profile_lighting_v1_limits_t limits = noah_profile_lighting_v1_default_limits();
    noah_profile_lighting_v1_view_t   view;
    assert(noah_profile_lighting_v1_decode(bytes, program.length, &limits, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view, &program));
    noah_lighting_history_bank_begin(&bank, &program, &arena);
    noah_lighting_facts_t facts = {0};
    for (unsigned key = 0; key < 60; key++)
        facts.key_pressed[key / 8] |= 1u << (key % 8);
    assert(noah_lighting_history_observe(&bank, &facts, 1, scratch));
    assert(noah_lighting_history_store_freeze(&bank.store));
    uint32_t frozen_bytes = arena.used;
    memset(facts.key_pressed, 0, sizeof facts.key_pressed);
    facts.key_pressed[7] = 8u;
    assert(noah_lighting_history_bank_advance(&bank, 1));
    assert(noah_lighting_history_observe(&bank, &facts, 2, scratch));
    facts.key_pressed[0] = 1u;
    assert(noah_lighting_history_observe(&bank, &facts, 3, scratch));
    for (unsigned rule = 0; rule < 255; rule++) {
        noah_lighting_retained_t current, frozen;
        assert(noah_lighting_history_store_read(&bank.store, NOAH_LIGHTING_ARENA_CURRENT, rule, 0, &current));
        assert(noah_lighting_history_store_read(&bank.store, NOAH_LIGHTING_ARENA_FRAME, rule, 0, &frozen));
        assert(noah_lighting_retained_order(&current) == 3 && noah_lighting_retained_order(&frozen) == 1);
        for (unsigned thing = 1; thing < 60; thing++) {
            assert(noah_lighting_history_store_read(&bank.store, NOAH_LIGHTING_ARENA_CURRENT, rule, thing, &current));
            assert(noah_lighting_retained_order(&current) == 1);
        }
    }
    assert(noah_lighting_history_bank_advance(&bank, 65534));
    assert(noah_lighting_history_bank_advance(&bank, 1));
    for (unsigned rule = 0; rule < 255; rule++) {
        noah_lighting_retained_t cell;
        for (unsigned key = 0; key < 60; key++) {
            bool current = noah_lighting_history_store_read(&bank.store, NOAH_LIGHTING_ARENA_CURRENT, rule, key, &cell);
            assert(current == (key == 0 || key == 59));
            assert(noah_lighting_history_store_read(&bank.store, NOAH_LIGHTING_ARENA_FRAME, rule, key, &cell));
        }
    }
    noah_lighting_history_store_clear(&bank.store, NOAH_LIGHTING_ARENA_CURRENT);
    noah_lighting_history_store_clear(&bank.store, NOAH_LIGHTING_ARENA_FRAME);
    assert(!arena.used);
    printf("coherent observation full-domain sample:255 rules x60 keys, reentry/expiry/frozen independence; current+frame checkpoint%uB (not worst accepted capacity)\n", frozen_bytes);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = fopen(argv[1], "r");
    assert(file);
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    noah_lighting_history_store_init(&bank.store, &arena);
    char     word[32], name[128];
    unsigned cases = 0, steps = 0;
    uint32_t maximum = 0;
    while (fscanf(file, "%31s", word) == 1) {
        if (!strcmp(word, "CASE")) {
            noah_lighting_history_store_clear(&bank.store, NOAH_LIGHTING_ARENA_FRAME);
            noah_lighting_history_store_clear(&bank.store, NOAH_LIGHTING_ARENA_CURRENT);
            assert(!arena.used);
            unsigned length;
            assert(fscanf(file, "%127s %u", name, &length) == 2);
            static uint8_t bytes[8192];
            assert(length <= sizeof bytes);
            readhex(file, bytes, length);
            noah_profile_lighting_v1_limits_t limits = noah_profile_lighting_v1_default_limits();
            noah_profile_lighting_v1_view_t   view;
            assert(noah_profile_lighting_v1_decode(bytes, length, &limits, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
            assert(noah_profile_lighting_v1_program_load(&view, &program));
            noah_lighting_history_bank_begin(&bank, &program, &arena);
            cases++;
            continue;
        }
        unsigned long delta;
        uint64_t      order;
        unsigned      layers, behaviours;
        assert(!strcmp(word, "FRAME") && fscanf(file, "%lu %" SCNu64 " %u %u", &delta, &order, &layers, &behaviours) == 4);
        noah_lighting_facts_t facts = {0};
        facts.layer_active[0]       = layers;
        facts.layer_active[1]       = layers >> 8;
        for (unsigned i = 0; i < behaviours; i++) {
            unsigned key, id, slot, outcome;
            assert(fscanf(file, "%u %u %u %u", &key, &id, &slot, &outcome) == 4);
            assert(key < 60 && id < 128 && slot < 5);
            uint8_t *at = facts.behaviour + key * KEY_FEEDBACK_LIGHTING_RECORD_BYTES;
            at[0]       = id + 1;
            at[1]       = slot + 1;
            at[2]       = outcome;
        }
        readhex(file, facts.key_pressed, sizeof facts.key_pressed);
        noah_lighting_facts_t before = facts;
        assert(noah_lighting_history_bank_advance(&bank, (uint32_t)delta));
        assert(noah_lighting_history_observe(&bank, &facts, order, scratch));
        assert(!memcmp(&facts, &before, sizeof facts));
        assert(noah_lighting_history_store_freeze(&bank.store));
        if (arena.used > maximum) maximum = arena.used;
        unsigned count;
        assert(fscanf(file, "%u", &count) == 1);
        noah_lighting_history_cursor_t cursor;
        noah_lighting_history_store_cursor(&bank.store, &cursor);
        for (unsigned i = 0; i < count; i++) {
            unsigned rule, thing, visible, wake, members;
            uint64_t expected_order;
            assert(fscanf(file, "%u %u %u %u %" SCNu64 " %u", &rule, &thing, &visible, &wake, &expected_order, &members) == 6);
            noah_lighting_retained_t cell;
            assert(noah_lighting_history_store_next(&bank.store, NOAH_LIGHTING_ARENA_FRAME, &cursor, &cell));
            assert(noah_lighting_retained_rule(&cell) == rule && noah_lighting_retained_thing(&cell) == thing);
            noah_lighting_history_rule_t info;
            assert(noah_lighting_history_rule(&program, rule, &info));
            if (noah_lighting_retained_order(&cell) != expected_order || noah_lighting_retained_visible(&cell, &info.timing) != !!visible || noah_lighting_retained_wake(&cell, &info.timing) != wake) {
                fprintf(stderr, "%s step%u rule%u/thing%u metadata mismatch\n", name, steps, rule, thing);
                return 1;
            }
            noah_lighting_batch_ref_t          ref    = noah_lighting_retained_batch(&cell);
            noah_lighting_owned_batch_reader_t reader = {0};
            if (members)
                assert(noah_lighting_owned_batch_begin(&arena, ref, &reader) && reader.remaining == members);
            else
                assert(!ref);
            for (unsigned m = 0; m < members; m++) {
                unsigned kind, id, slot, outcome, sides, has_keys;
                uint8_t  keys[KEY_ORIGIN_BITMAP_SIZE];
                assert(fscanf(file, "%u %u %u %u %u %u", &kind, &id, &slot, &outcome, &sides, &has_keys) == 6);
                readhex(file, keys, sizeof keys);
                noah_lighting_subject_t subject;
                assert(noah_lighting_owned_batch_next(&arena, &reader, &subject));
                assert(subject.thing_kind == kind && subject.thing == id && subject.slot == slot && subject.outcome == outcome && subject.sides == sides && subject.has_keys == !!has_keys && !memcmp(subject.keys, keys, sizeof keys));
            }
        }
        noah_lighting_retained_t extra;
        assert(!noah_lighting_history_store_next(&bank.store, NOAH_LIGHTING_ARENA_FRAME, &cursor, &extra));
        noah_lighting_history_store_clear(&bank.store, NOAH_LIGHTING_ARENA_FRAME);
        steps++;
    }
    noah_lighting_history_store_clear(&bank.store, NOAH_LIGHTING_ARENA_CURRENT);
    assert(!arena.used);
    fclose(file);
    full_rule_domain();
    printf("coherent state history: %u cases/%u frames, actual fact walkers/filter loss/partial departures/frozen ordered owners/show/linger/high rule+thing PASS; sample arena peak%uB (not target capacity)\n", cases, steps, maximum);
}
