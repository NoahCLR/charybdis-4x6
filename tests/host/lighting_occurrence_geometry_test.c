#include "users/noah/lib/rgb/lighting/lighting_occurrence.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

bool noah_lighting_fact_next(uint8_t kind, const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    (void)kind;
    (void)facts;
    (void)params;
    (void)cursor;
    (void)subject;
    assert(!"no state filters in fixture");
    return false;
}
int main(void) {
    // Same accepted positive macro-event+explicit flash profile as shared
    // construction/occurrence fixtures; no weakened validator or test-only kind.
    const uint8_t                             payload[] = {0x05, 0x00, 0x3a, 0, 0, 0, 0, 1, 0x0d, 0, 0xff, 1, 28, 0xff, 0x40, 4, 0x60, 0, 0xa2, 3, 0xf4, 1, 0};
    static noah_profile_lighting_v1_program_t program;
    noah_profile_lighting_v1_view_t           view;
    noah_profile_lighting_v1_limits_t         limits = noah_profile_lighting_v1_default_limits();
    assert(noah_profile_lighting_v1_decode(payload, sizeof payload, &limits, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
    assert(noah_profile_lighting_v1_program_load(&view, &program));
    uint8_t owners[KEY_ORIGIN_BITMAP_SIZE];
    memset(owners, 0, sizeof owners);
    owners[0] = 1;
    owners[(MATRIX_ROWS * MATRIX_COLS - 1u) / 8u] |= (uint8_t)(1u << ((MATRIX_ROWS * MATRIX_COLS - 1u) % 8u));
    noah_lighting_event_t       event = {.kind = NOAH_LIGHTING_EVENT_MACRO_PLAYED, .thing = 127, .slot = 2, .outcome = 3, .owners = owners};
    noah_lighting_facts_t       facts = {0};
    noah_lighting_event_group_t group;
    assert(noah_lighting_event_qualify(&program, &facts, &event, 123, &group));
    assert(group.thing == 127 && group.slot == 2 && group.outcome == 3 && group.qualified[0] == 1);
    assert(!memcmp(group.owners, owners, sizeof owners));
    for (size_t i = sizeof owners; i < sizeof group.owners; i++)
        assert(group.owners[i] == 0);
    memset(owners, 0, sizeof owners);
    assert(group.owners[0] == 1);
    printf("occurrence owner geometry %ux%u: copies%zu bytes, zero-extends fixed8-byte transport, borrowed data expired PASS\n", MATRIX_ROWS, MATRIX_COLS, sizeof owners);
}
