#include "users/noah/lib/rgb/lighting/lighting_history_owner.h"
#include "users/noah/lib/rgb/lighting/lighting_owned_batch.h"
#include "users/noah/lib/rgb/lighting/lighting_history_work.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static uint64_t work[NOAH_LIGHTING_WORK_KINDS];
void            noah_lighting_history_work(uint8_t kind, uint32_t amount) {
    assert(kind < NOAH_LIGHTING_WORK_KINDS);
    work[kind] += amount;
}
// Full logical-domain HOST metadata deliberately tests independent transitions.
// This is not a target reservation, a physical reachability/capacity proof or a
// production arena choice. The actual producer integration covers qualification.
static noah_lighting_retained_t           cells[2][255u * 128u];
static noah_lighting_history_index_t      index_table;
static noah_profile_lighting_v1_program_t program;
static noah_lighting_history_owner_t      owner[2];
static noah_lighting_batch_arena_t        arena[2];
static uint8_t                            payload[8192], banks[2][16384];
static void                               full_mask(noah_lighting_event_group_t *group) {
    memset(group->qualified, 255, sizeof group->qualified);
    group->qualified[31] = 127;
}
static noah_lighting_event_group_t event(uint8_t thing, uint64_t order) {
    noah_lighting_event_group_t group = {.order = order, .thing_kind = 5, .thing = thing, .slot = 2, .outcome = 17, .has_keys = 1, .sides = 1};
    group.owners[0]                   = 1;
    full_mask(&group);
    return group;
}
static void apply(unsigned half, const noah_lighting_event_group_t *group) {
    uint8_t scratch[NOAH_LIGHTING_GROUP_BATCH_BYTES];
    assert(noah_lighting_history_owner_event(&owner[half], group, half == 0, scratch));
    memset(scratch, 0xa5, sizeof scratch);
}
static const noah_lighting_retained_t *cell(unsigned half, unsigned rule, unsigned thing) {
    const noah_lighting_retained_t *found = noah_lighting_history_owner_find(&owner[half], rule, thing);
    assert(found);
    return found;
}
static noah_lighting_lifetime_parameters_t parameters(unsigned rule) {
    noah_lighting_history_rule_t description;
    assert(noah_lighting_history_rule(&program, rule, &description));
    return description.timing;
}
static bool visible(unsigned half, unsigned rule, unsigned thing) {
    noah_lighting_lifetime_parameters_t timing = parameters(rule);
    const noah_lighting_retained_t     *found  = noah_lighting_history_owner_find(&owner[half], rule, thing);
    return found && noah_lighting_retained_visible(found, &timing);
}
int main(void) {
    uint8_t header[] = {5, 0, 58, 0, 0, 0, 0, 255};
    memcpy(payload, header, sizeof header);
    size_t used = sizeof header;
    for (unsigned rule = 0; rule < 255; rule++) {
        uint16_t show = (rule % 3) * 100, flash = 500 + (rule % 5) * 100;
        uint8_t  bytes[] = {(show ? 17u : 13u), 0, 255, 1, 28, 255, 0x40, 4, 0x60, 0};
        memcpy(payload + used, bytes, sizeof bytes);
        used += sizeof bytes;
        if (show) {
            uint8_t gate[] = {0xa2, 1, (uint8_t)show, (uint8_t)(show >> 8)};
            memcpy(payload + used, gate, sizeof gate);
            used += sizeof gate;
        }
        uint8_t window[] = {0xa2, 3, (uint8_t)flash, (uint8_t)(flash >> 8)};
        memcpy(payload + used, window, sizeof window);
        used += sizeof window;
    }
    payload[used++]                          = 0;
    noah_profile_lighting_v1_limits_t limits = noah_profile_lighting_v1_default_limits();
    noah_profile_lighting_v1_view_t   view;
    assert(noah_profile_lighting_v1_decode(payload, used, &limits, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view, &program));
    assert(noah_lighting_history_index_prepare(&program, &index_table));
    assert(index_table.offsets[255] == 255u * 128u);
    for (unsigned half = 0; half < 2; half++) {
        noah_lighting_batch_arena_init(&arena[half], banks[half], sizeof banks[half]);
        memset(&owner[half], 0xa5, sizeof owner[half]);
        noah_lighting_history_owner_t before = owner[half];
        assert(!noah_lighting_history_owner_begin(&owner[half], &program, &index_table, NULL, 1, &arena[half]));
        assert(!memcmp(&before, &owner[half], sizeof before));
        assert(noah_lighting_history_owner_begin(&owner[half], &program, &index_table, cells[half], 255u * 128u, &arena[half]));
        assert(!owner[half].count);
    }
    noah_lighting_event_group_t group = event(127, 1), sent;
    owner[0].capacity                 = 254u;
    uint8_t preflight[NOAH_LIGHTING_GROUP_BATCH_BYTES];
    assert(!noah_lighting_history_owner_event(&owner[0], &group, true, preflight));
    assert(!owner[0].count && !arena[0].used);
    owner[0].capacity = 255u * 128u;
    apply(0, &group);
    assert(owner[0].count == 255u);
    assert(arena[0].used == 17u);
    assert(noah_lighting_history_owner_next(&owner[0], &sent) == NOAH_LIGHTING_HISTORY_READY);
    uint8_t sent_bytes[NOAH_LIGHTING_EVENT_GROUP_BYTES], expected_bytes[NOAH_LIGHTING_EVENT_GROUP_BYTES];
    assert(noah_lighting_event_group_encode(sent_bytes, sizeof sent_bytes, &sent));
    assert(noah_lighting_event_group_encode(expected_bytes, sizeof expected_bytes, &group));
    assert(!memcmp(sent_bytes, expected_bytes, sizeof sent_bytes));
    noah_lighting_retained_t frozen;
    assert(noah_lighting_retained_pin(cell(0, 254, 127), &arena[0], &frozen));
    noah_lighting_history_owner_advance(&owner[0], 100);
    // A later nonqualifying occurrence does not invoke the owner with a new
    // qualified bit. Its old marker, age and immutable payload stay owned.
    assert(noah_lighting_retained_order(cell(0, 254, 127)) == 1);
    noah_lighting_event_group_t repeat = event(127, 2);
    memset(repeat.qualified, 0, 32);
    repeat.qualified[0] = 1;
    repeat.slot         = 7;
    repeat.outcome      = 33;
    repeat.owners[0]    = 0;
    repeat.owners[7]    = 8;
    repeat.sides        = 2;
    apply(0, &repeat);
    noah_lighting_history_owner_ack(&owner[0], &sent);
    assert(noah_lighting_retained_dirty(cell(0, 0, 127)) && !noah_lighting_retained_dirty(cell(0, 254, 127)));
    assert(noah_lighting_retained_order(cell(0, 0, 127)) == 2);
    noah_lighting_history_owner_advance(&owner[0], 1000);
    assert(!visible(0, 0, 127));
    assert(noah_lighting_history_owner_next(&owner[0], &sent) == NOAH_LIGHTING_HISTORY_READY && sent.order == 2 && sent.qualified[0] == 1);
    // Deliver source-expired data to the other half's first local observation.
    apply(1, &sent);
    assert(visible(1, 0, 127));
    noah_lighting_history_owner_advance(&owner[1], 499);
    apply(1, &sent);
    assert(visible(1, 0, 127));
    noah_lighting_history_owner_advance(&owner[1], 1);
    assert(!visible(1, 0, 127));
    noah_lighting_history_owner_session(&owner[0], true);
    assert(noah_lighting_history_owner_next(&owner[0], &sent) == NOAH_LIGHTING_HISTORY_EMPTY);
    assert(!owner[0].count);
    group = event(127, 3);
    apply(0, &group);
    assert(!visible(0, 254, 127)); // still waiting show-after
    noah_lighting_history_owner_session(&owner[0], true);
    assert(noah_lighting_history_owner_next(&owner[0], &sent) == NOAH_LIGHTING_HISTORY_READY && sent.order == 3);
    noah_lighting_history_owner_ack(&owner[0], &sent);
    noah_lighting_history_owner_advance(&owner[0], 1000);
    // Frozen generation still owns the exact older key/slot/outcome payload.
    noah_lighting_owned_batch_reader_t reader;
    noah_lighting_subject_t            subject;
    assert(noah_lighting_owned_batch_begin(&arena[0], noah_lighting_retained_batch(&frozen), &reader));
    assert(noah_lighting_owned_batch_next(&arena[0], &reader, &subject));
    assert(subject.keys[0] == 1 && subject.slot == 2 && subject.outcome == 17);
    noah_lighting_retained_discard(&frozen, &arena[0]);
    assert(!arena[0].used);
    // Reserve failure is transactional at this seam, never an overflow policy.
    group = event(127, 4);
    group.qualified[31] |= 128;
    assert(!noah_lighting_history_owner_event(&owner[0], &group, true, payload));
    assert(!arena[0].used);
    group            = event(127, 4);
    group.thing_kind = 1;
    assert(!noah_lighting_history_owner_event(&owner[0], &group, true, payload));
    assert(!arena[0].used);
    // Failure after metadata preflight must remove only new empty records;
    // old visible/current ownership survives arena reserve failure unchanged.
    noah_lighting_history_owner_t small;
    noah_lighting_retained_t      small_cells[2];
    noah_lighting_batch_arena_t   small_arena;
    uint8_t                       small_bytes[17];
    noah_lighting_batch_arena_init(&small_arena, small_bytes, sizeof small_bytes);
    assert(noah_lighting_history_owner_begin(&small, &program, &index_table, small_cells, 2, &small_arena));
    group = event(127, 20);
    memset(group.qualified, 0, 32);
    group.qualified[0] = 1;
    uint8_t small_scratch[NOAH_LIGHTING_GROUP_BATCH_BYTES];
    assert(noah_lighting_history_owner_event(&small, &group, true, small_scratch));
    noah_lighting_retained_t before = *noah_lighting_history_owner_find(&small, 0, 127);
    group                           = event(127, 21);
    memset(group.qualified, 0, 32);
    group.qualified[31] = 64;
    group.slot          = 99;
    assert(!noah_lighting_history_owner_event(&small, &group, true, small_scratch));
    assert(small.count == 1 && small_arena.used == 17 && !noah_lighting_history_owner_find(&small, 254, 127));
    assert(!memcmp(&before, noah_lighting_history_owner_find(&small, 0, 127), sizeof before));
    noah_lighting_history_owner_discard(&small);
    assert(!small_arena.used);
    // New lower keys must not move the fair cursor back ahead of an older
    // dirty subject. ACK is version-specific, independent of insertion order.
    group = event(10, 30);
    memset(group.qualified, 0, 32);
    group.qualified[0] = 1;
    apply(0, &group);
    group = event(20, 31);
    memset(group.qualified, 0, 32);
    group.qualified[0] = 1;
    apply(0, &group);
    assert(noah_lighting_history_owner_next(&owner[0], &sent) == NOAH_LIGHTING_HISTORY_READY && sent.thing == 10);
    noah_lighting_history_owner_ack(&owner[0], &sent);
    group = event(5, 32);
    memset(group.qualified, 0, 32);
    group.qualified[0] = 1;
    apply(0, &group);
    assert(noah_lighting_history_owner_next(&owner[0], &sent) == NOAH_LIGHTING_HISTORY_READY && sent.thing == 20);
    noah_lighting_history_owner_ack(&owner[0], &sent);
    assert(noah_lighting_history_owner_next(&owner[0], &sent) == NOAH_LIGHTING_HISTORY_READY && sent.thing == 5);
    noah_lighting_history_owner_ack(&owner[0], &sent);
    noah_lighting_history_owner_discard(&owner[0]);
    assert(!arena[0].used);
    // Every accepted rule/thing address, independent durations, high ids and
    // natural occurrence versions. Equal full batches share bytes, not clocks.
    for (unsigned thing = 0; thing < 128; thing++) {
        group      = event(thing, 1000 + thing);
        group.slot = thing;
        apply(0, &group);
    }
    for (unsigned rule = 0; rule < 255; rule++)
        for (unsigned thing = 0; thing < 128; thing++)
            assert(noah_lighting_retained_order(cell(0, rule, thing)) == 1000 + thing && noah_lighting_retained_dirty(cell(0, rule, thing)));
    bool seen[128] = {0};
    for (unsigned thing = 0; thing < 128; thing++) {
        assert(noah_lighting_history_owner_next(&owner[0], &sent) == NOAH_LIGHTING_HISTORY_READY);
        assert(!seen[sent.thing]);
        seen[sent.thing] = true;
        for (unsigned rule = 0; rule < 255; rule++)
            assert(sent.qualified[rule / 8] & (1u << (rule % 8)));
        noah_lighting_history_owner_ack(&owner[0], &sent);
    }
    assert(noah_lighting_history_owner_next(&owner[0], &sent) == NOAH_LIGHTING_HISTORY_EMPTY);
    // Bulk qualification must reserve/move records in one ordered merge, not
    // shift the whole bank once for each qualified rule. This bounds copied
    // bytes for this full accepted-domain trace, not target execution time.
    assert(work[NOAH_LIGHTING_WORK_MOVE] < 80000000u);
    printf("history owner operation counts: comparisons=%" PRIu64 " moved-byte-arguments=%" PRIu64 " calendars=%" PRIu64 " descriptor-decodes=%" PRIu64 " dirty-visits=%" PRIu64 " (host trace, not target time)\n", work[0], work[1], work[2], work[3], work[4]);
    for (unsigned half = 0; half < 2; half++) {
        noah_lighting_history_owner_discard(&owner[half]);
        assert(!arena[half].used);
    }
    printf("profile-owned histories: accepted %zuB/255-rule profile, all32640 logical cells, fair grouped dirty/ACK/repeat/expiry/local half/reset/frozen references normal; host metadata=%zuB/half, payload bank fixture=%zuB (NOT target allocation/bound) PASS\n", used, sizeof cells[0], sizeof banks[0]);
}
