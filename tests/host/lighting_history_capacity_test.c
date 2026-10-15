#include "users/noah/lib/rgb/lighting/lighting_history_bank.h"
#include "users/noah/lib/rgb/lighting/lighting_occurrence.h"
#include "matrix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static noah_profile_lighting_v1_program_t program;
static noah_lighting_history_bank_t       bank;
static noah_lighting_batch_arena_t        arena;
static uint32_t                           allocation_peak;
void                                      noah_lighting_arena_usage(uint32_t used) {
    if (used > allocation_peak) allocation_peak = used;
}
static uint8_t storage[24576], triples[255][3];
matrix_row_t   matrix_get_row(uint8_t row) {
    (void)row;
    return 0;
}
bool noah_lighting_fact_next(uint8_t kind, const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    assert(kind == 14);
    return noah_lighting_fact_key_pressed_next(facts, params, cursor, subject);
}
int main(void) {
    const unsigned rule_count  = 43u;
    uint8_t        bytes[8192] = {5, 0, 58, 0, 0, 0, 0, 0};
    unsigned       used = 8, rule = 0;
    bytes[7] = (uint8_t)rule_count;
    for (unsigned a = 0; a < 14 && rule < rule_count; a++)
        for (unsigned b = a + 1; b < 14 && rule < rule_count; b++)
            for (unsigned c = b + 1; c < 14 && rule < rule_count; c++) {
                const uint8_t row[] = {26, 0, 255, 1, 28, 255, 1, 14, a, 1, 14, b, 1, 14, c, 0x40, 0, 0x60, 0, 0xa2, 3, 1, 0};
                // 2 header bytes + trigger3 + filters3*3 + target2 + paint2 + flash4 =22 body bytes.
                uint8_t record[sizeof row];
                memcpy(record, row, sizeof row);
                record[0] = sizeof record - 1;
                memcpy(bytes + used, record, sizeof record);
                used += sizeof record;
                triples[rule][0]   = a;
                triples[rule][1]   = b;
                triples[rule++][2] = c;
            }
    bytes[used++]                            = 0;
    noah_profile_lighting_v1_limits_t limits = noah_profile_lighting_v1_default_limits();
    noah_profile_lighting_v1_view_t   view;
    assert(noah_profile_lighting_v1_decode(bytes, used, &limits, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view, &program));
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    noah_lighting_history_bank_begin(&bank, &program, &arena);
    unsigned accepted = 0;
    uint64_t order    = 1;
    uint32_t peak     = 0u;
    for (unsigned thing = 0; thing < 128; thing++)
        for (unsigned r = 0; r < rule_count; r++) {
            noah_lighting_facts_t facts = {0};
            for (unsigned i = 0; i < 3; i++)
                facts.key_pressed[triples[r][i] / 8] |= 1u << (triples[r][i] % 8);
            noah_lighting_event_t       event = {.kind = NOAH_LIGHTING_EVENT_MACRO_PLAYED, .thing = thing, .key = {.row = MATRIX_ROWS, .col = MATRIX_COLS}};
            noah_lighting_event_group_t group;
            assert(noah_lighting_event_qualify(&program, &facts, &event, order++, &group));
            for (unsigned byte = 0; byte < 32; byte++)
                assert(group.qualified[byte] == (byte == r / 8 ? (1u << (r % 8)) : 0u));
            uint8_t scratch[1 + 3 + KEY_ORIGIN_BITMAP_SIZE];
            if (accepted == 500u) assert(noah_lighting_history_bank_advance(&bank, 65535u));
            uint32_t before = arena.used;
            if (!noah_lighting_history_bank_event(&bank, &group, true, scratch)) {
                assert(arena.used == before);
                unsigned                       current = 0, expired = 0;
                noah_lighting_history_cursor_t cursor;
                noah_lighting_retained_t       cell;
                noah_lighting_history_store_cursor(&bank.store, &cursor);
                while (noah_lighting_history_store_next(&bank.store, NOAH_LIGHTING_ARENA_CURRENT, &cursor, &cell)) {
                    assert(noah_lighting_retained_dirty(&cell));
                    noah_lighting_lifetime_parameters_t p = {.flash = 1};
                    expired += !noah_lighting_retained_visible(&cell, &p);
                    current++;
                }
                assert(current == accepted);
                printf("factored capacity regression unexpected failure: accepted profile%uB/%u rules, distinct3-of14 physical-key filters, first reserve failure occurrence%u thing%u rule%u; coherent prior%u records intact; current bytes%u capacity%u. No frozen frame yet; source markers DIRTY, %u expired.\n", used, rule_count, accepted + 1, thing, r, current, arena.used, arena.capacity, expired);
                printf("This exercises actual validator/optional event matcher/coherent key walker/qualifier/bank; NOT a full QMK-input/hardware reachability proof, nor a physical lower bound for better representations.\n");
                return 2;
            }
            accepted++;
            if (arena.used > peak) peak = arena.used;
        }
    printf("FACTORED candidate completes%u rules x128 things =%u qualified pairs; profile%uB; current arena%uB, measured trace peak%uB, allocation%uB, store control%zu bank%zu arena-control%zu host (target layouts separately); no final accepted-bound claim.\n", rule_count, accepted, used, arena.used, peak, arena.capacity, sizeof bank.store, sizeof bank, sizeof arena);
    uint32_t                  batch_headers = 0, batch_bytes = 0, meta_headers = 0, meta_fixed = 0, rounding_bits = 0;
    uint64_t                  columns[4] = {0};
    noah_lighting_batch_ref_t ref;
    uint32_t                  offset = 0;
    while (noah_lighting_record_next(&arena, NOAH_LIGHTING_ARENA_BATCH, &offset, &ref)) {
        uint16_t length;
        assert(noah_lighting_batch_describe(&arena, ref, NULL, NULL, &length, NULL));
        batch_headers += 12;
        batch_bytes += length;
    }
    offset = 0;
    while (noah_lighting_record_next(&arena, NOAH_LIGHTING_ARENA_CURRENT, &offset, &ref)) {
        uint8_t  header[52];
        uint16_t length;
        assert(noah_lighting_batch_describe(&arena, ref, NULL, NULL, &length, NULL));
        assert(noah_lighting_batch_copy(&arena, ref, 0, header, sizeof header));
        unsigned count = 0;
        for (unsigned r = 0; r < 255; r++)
            count += !!(header[r / 8] & (1u << (r % 8)));
        unsigned fieldbits = 0;
        for (unsigned field = 0; field < 4; field++) {
            columns[field] += count * header[48 + field];
            fieldbits += count * header[48 + field];
        }
        meta_headers += 12;
        meta_fixed += 52;
        rounding_bits += (8 - fieldbits % 8) % 8;
    }
    printf("LEDGER current payload headers%u/body%u; assignment block headers%u/fixed%u; columnbits order%llu ref%llu lifetime%llu flags%llu pad%u; transient arena high%uB\n", batch_headers, batch_bytes, meta_headers, meta_fixed, (unsigned long long)columns[0], (unsigned long long)columns[1], (unsigned long long)columns[2], (unsigned long long)columns[3], rounding_bits, allocation_peak);
    assert(noah_lighting_history_store_freeze(&bank.store));
    unsigned                       live_frame = 0;
    noah_lighting_retained_t       cell;
    noah_lighting_history_cursor_t cursor;
    noah_lighting_history_store_cursor(&bank.store, &cursor);
    while (noah_lighting_history_store_next(&bank.store, NOAH_LIGHTING_ARENA_FRAME, &cursor, &cell)) {
        assert(noah_lighting_retained_rendering(&cell));
        live_frame++;
    }
    printf("FROZEN frame consumable records%u (expired500 absent), shared/current+frame arena%uB peak%uB\n", live_frame, arena.used, allocation_peak);
    noah_lighting_event_group_t pending;
    assert(noah_lighting_history_bank_next(&bank, &pending));
    noah_lighting_event_group_t old = pending;
    pending.order                   = UINT64_C(0x100000001);
    uint8_t scratch[1 + 3 + KEY_ORIGIN_BITMAP_SIZE];
    assert(noah_lighting_history_bank_event(&bank, &pending, true, scratch));
    assert(noah_lighting_history_bank_ack(&bank, &old));
    for (unsigned r = 0; r < rule_count; r++)
        if (pending.qualified[r / 8] & (1u << (r % 8))) {
            assert(noah_lighting_history_store_read(&bank.store, NOAH_LIGHTING_ARENA_CURRENT, r, pending.thing, &cell));
            assert(noah_lighting_retained_order(&cell) == pending.order && noah_lighting_retained_dirty(&cell));
        }
    uint32_t cap                       = arena.capacity;
    arena.capacity                     = arena.used;
    noah_lighting_event_group_t failed = pending;
    failed.order++;
    assert(!noah_lighting_history_bank_event(&bank, &failed, true, scratch));
    arena.capacity = cap;
    for (unsigned r = 0; r < rule_count; r++)
        if (pending.qualified[r / 8] & (1u << (r % 8))) {
            assert(noah_lighting_history_store_read(&bank.store, NOAH_LIGHTING_ARENA_CURRENT, r, pending.thing, &cell));
            assert(noah_lighting_retained_order(&cell) == pending.order);
        }
    printf("OVERLAP pending oldwire u64%llu/newversion%llu: exact ACK isolation/reserve rollback; arena%uB high%uB; no linked allocation claim\n", (unsigned long long)old.order, (unsigned long long)pending.order, arena.used, allocation_peak);
    noah_lighting_history_store_clear(&bank.store, NOAH_LIGHTING_ARENA_FRAME);
    noah_lighting_history_store_clear(&bank.store, NOAH_LIGHTING_ARENA_CURRENT);
    assert(!arena.used);
    return 0;
}
