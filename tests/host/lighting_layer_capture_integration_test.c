// Reuse the complete real ownership suite and its QMK helpers. Its recording
// observer still verifies actual transition ordering/IDs/borrowed owners.
#define noah_lighting_event_observe recording_observer
#define main ownership_suite
#include "layer_ownership_test.c"
#undef main
#undef noah_lighting_event_observe
#include "users/noah/lib/rgb/lighting/lighting_event_capture.h"
#include "users/noah/lib/rgb/lighting/lighting_workspace.h"
#include "users/noah/lib/rgb/lighting/lighting_retained_group.h"
#include "users/noah/lib/rgb/lighting/lighting_history_owner.h"
#include "users/noah/lib/rgb/lighting/lighting_owned_batch.h"
#include <assert.h>

noah_lighting_workspace_t                  noah_lighting_workspace;
static noah_profile_lighting_v1_program_t  program;
static noah_lighting_batch_arena_t         arena;
static noah_lighting_history_index_t       index_table;
static noah_lighting_history_owner_t       history;
static noah_lighting_retained_t            cells[3];
static uint8_t                             storage[512];
static unsigned                            depth, captures, collections;
static uint64_t                            natural_order;
static noah_lighting_lifetime_parameters_t parameters = {.flash = 500};
uint8_t                                    get_highest_layer(layer_state_t state) {
    uint8_t highest = 0;
    while (state >>= 1u)
        highest++;
    return highest;
}

bool noah_lighting_fact_next(uint8_t kind, const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    switch (kind) {
        case NOAH_LIGHTING_FACT_ID_LAYER_ACTIVE:
            return noah_lighting_fact_layer_active_next(facts, params, cursor, subject);
        case NOAH_LIGHTING_FACT_ID_LAYER_STATUS:
            return noah_lighting_fact_layer_status_next(facts, params, cursor, subject);
        default:
            assert(!"unexpected state fixture kind");
            return false;
    }
}
static void collect(noah_lighting_facts_t *facts) {
    assert(depth == 1);
    collections++;
    memset(facts, 0, sizeof *facts);
    noah_lighting_slice_layer_active_collect(facts->layer_active);
    noah_lighting_slice_layer_status_collect(facts->layer_status);
    assert(depth == 1); // collectors didn't invoke a producer
}
static void retain(void *context, const noah_lighting_event_group_t *group, uint32_t now) {
    (void)context;
    assert(depth == 1 && now == 100);
    assert(noah_lighting_history_owner_event(&history, group, true, noah_lighting_workspace.capture.encoded));
    captures++;
}
void noah_lighting_event_observe(const noah_lighting_event_t *event) {
    assert(depth == 0);
    depth++;
    recording_observer(event);
    if (program.valid) (void)noah_lighting_event_capture_shared(&program, event, ++natural_order, 100, collect, retain, NULL);
    depth--;
}
int main(void) {
    assert(ownership_suite() == 0);
    noah_lighting_batch_arena_init(&arena, storage, sizeof storage);
    for (unsigned rule = 0; rule < 3; rule++)
        noah_lighting_retained_init(&cells[rule], rule, 3);
    // Positive lock filtered by actual locked state; unlock filtered by NOT
    // locked. A second lock rule requires a different actually held layer,
    // proving no later resurrection when that filter changes after occurrence.
    static const uint8_t              bytes[] = {5, 0, 58, 0, 0, 0, 0, 3, 17, 0, 255, 1, 19, 3, 2, 9, 3, 2, 0x40, 4, 0x60, 0, 0xa2, 3, 0xf4, 1, 17, 0, 255, 1, 20, 3, 0x22, 9, 3, 2, 0x40, 4, 0x60, 0, 0xa2, 3, 0xf4, 1, 17, 0, 255, 1, 19, 3, 2, 9, 4, 1, 0x40, 4, 0x60, 0, 0xa2, 3, 0xf4, 1, 0};
    noah_profile_lighting_v1_limits_t limits  = noah_profile_lighting_v1_default_limits();
    noah_profile_lighting_v1_view_t   view;
    assert(noah_profile_lighting_v1_decode(bytes, sizeof bytes, &limits, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view, &program));
    assert(noah_lighting_history_index_prepare(&program, &index_table));
    assert(index_table.offsets[program.rule_count] == 3u);
    assert(noah_lighting_history_owner_begin(&history, &program, &index_table, cells, 3u, &arena));
    layer_ownership_reset_for_test();
    event_count    = 0;
    keypos_t owner = {.row = 1, .col = 2};
    layer_ownership_momentary_press(owner, 3);
    assert(layer_ownership_set_lock_state(3, true));
    assert(captures == 1 && collections == 1 && depth == 0);
    assert(noah_lighting_retained_visible(&cells[0], &parameters));
    assert(!noah_lighting_retained_batch(&cells[1]) && !noah_lighting_retained_batch(&cells[2]));
    noah_lighting_retained_t frozen;
    assert(noah_lighting_retained_pin(&cells[0], &arena, &frozen));
    // Poison all shared scratch once synchronous observation returns; the
    // producer's borrowed owners and the arena's encoded output are independent.
    memset(&noah_lighting_workspace, 0xa5, sizeof noah_lighting_workspace);
    layer_ownership_momentary_release(owner);
    keypos_t another = {.row = 2, .col = 3};
    layer_ownership_momentary_press(another, 4);
    assert(captures == 1 && !noah_lighting_retained_batch(&cells[2]));
    assert(!layer_ownership_set_lock_state(3, true));
    assert(captures == 1); // dedup
    assert(layer_ownership_set_lock_state(3, false));
    assert(captures == 2 && collections == 2);
    noah_lighting_owned_batch_reader_t reader;
    noah_lighting_subject_t            subject;
    assert(noah_lighting_owned_batch_begin(&arena, noah_lighting_retained_batch(&frozen), &reader));
    assert(noah_lighting_owned_batch_next(&arena, &reader, &subject));
    assert(key_origin_bitmap_has_keypos(subject.keys, owner) && subject.thing == 3);
    // Qualify the second lock rule while layer4 is actually held. After its
    // release, a later real lock repeat fails that filter; the older qualified
    // marker/order/dirty payload must survive (D2).
    assert(layer_ownership_set_lock_state(3, true));
    assert(captures == 3 && collections == 3);
    assert(noah_lighting_retained_order(&cells[2]) == 3 && noah_lighting_retained_dirty(&cells[2]));
    noah_lighting_batch_ref_t qualified = noah_lighting_retained_batch(&cells[2]);
    layer_ownership_momentary_release(another);
    assert(layer_ownership_set_lock_state(3, false));
    assert(captures == 4 && collections == 4);
    assert(layer_ownership_set_lock_state(3, true));
    assert(captures == 5 && collections == 5);
    assert(noah_lighting_retained_order(&cells[2]) == 3 && noah_lighting_retained_dirty(&cells[2]));
    assert(noah_lighting_retained_batch(&cells[2]) == qualified && noah_lighting_retained_visible(&cells[2], &parameters));
    noah_lighting_retained_discard(&frozen, &arena);
    for (unsigned rule = 0; rule < 3; rule++)
        noah_lighting_retained_discard(&cells[rule], &arena);
    assert(!arena.used && !depth);
    puts("actual layer producer->coherent collector->qualifier->owned group: no reentry/dedup/occurrence filters/borrowed owners/frozen reference PASS");
}
