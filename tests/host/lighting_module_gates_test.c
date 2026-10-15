// Optional Painting modules report and validate exactly their compiled kinds.
#include <assert.h>
#include <stdio.h>
#include "users/noah/lib/profile/schema/profile_lighting_v1.h"

static void check(const uint8_t *bytes, size_t length, const noah_profile_lighting_v1_limits_t *limits, bool enabled) {
    noah_profile_lighting_v1_view_t   view;
    noah_profile_lighting_v1_result_t result = noah_profile_lighting_v1_decode(bytes, length, limits, &view, NULL);
    assert(enabled ? result == NOAH_PROFILE_LIGHTING_V1_OK : result == NOAH_PROFILE_LIGHTING_V1_UNSUPPORTED_KIND);
}

int main(void) {
    noah_profile_lighting_v1_limits_t limits      = noah_profile_lighting_v1_default_limits();
    const uint8_t                     combo[]     = {5, 0, 58, 0, 0, 0, 0, 1, 10, 0, 255, 2, 7, 127, 255, 0x40, 0, 0x60, 0, 0};
    const uint8_t                     behaviour[] = {5, 0, 58, 0, 0, 0, 0, 1, 10, 0, 255, 2, 8, 127, 15, 0x40, 0, 0x60, 0, 0};
    const uint8_t                     outcome[]   = {5, 0, 58, 0, 0, 0, 0, 1, 6, 0, 255, 0x40, 0, 0x60, 8, 0};
    const uint8_t                     map[]       = {5, 0, 58, 0, 0, 0, 1, 0, 1, 1, 0, 0x88, 0};
    const uint8_t keys_that[] = {5,0,58,0,0,0,0,1,8,0,255,0x42,8,15,127,0x60,0,0};
    const uint8_t what_it_does[] = {5,0,58,0,0,0,0,1,6,0,255,0x40,0,0x60,9,0};
    assert(!!(limits.targets & (1u << 8)) == !!NOAH_LIGHTING_TARGET_KEYS_THAT);
    assert(!!(limits.paints & (1u << 9)) == !!NOAH_LIGHTING_PAINT_WHAT_IT_DOES);
    check(keys_that,sizeof(keys_that),&limits,NOAH_LIGHTING_TARGET_KEYS_THAT);
    check(what_it_does,sizeof(what_it_does),&limits,NOAH_LIGHTING_PAINT_WHAT_IT_DOES);
    assert(!!(limits.facts & (1u << 7)) == !!NOAH_LIGHTING_FACT_COMBO_HELD);
    assert(!!(limits.facts & (1u << 8)) == !!NOAH_LIGHTING_FACT_BEHAVIOUR);
    assert(!!(limits.paints & (1u << 8)) == !!NOAH_LIGHTING_PAINT_OUTCOME);
    check(combo, sizeof(combo), &limits, NOAH_LIGHTING_FACT_COMBO_HELD);
    check(behaviour, sizeof(behaviour), &limits, NOAH_LIGHTING_FACT_BEHAVIOUR);
    check(outcome, sizeof(outcome), &limits, NOAH_LIGHTING_PAINT_OUTCOME);
    // A map's unsupported value is a map-value error, not a rule-kind error.
    noah_profile_lighting_v1_view_t view;
    assert(noah_profile_lighting_v1_decode(map, sizeof(map), &limits, &view, NULL) == (NOAH_LIGHTING_PAINT_OUTCOME ? NOAH_PROFILE_LIGHTING_V1_OK : NOAH_PROFILE_LIGHTING_V1_INVALID_REFERENCE));
    const struct { uint8_t id, parameters; bool enabled; } events[] = {
        {19,1,NOAH_LIGHTING_FACT_LAYER_LOCKED}, {20,1,NOAH_LIGHTING_FACT_LAYER_UNLOCKED}, {21,1,NOAH_LIGHTING_FACT_ONESHOT_USED},
        {22,1,NOAH_LIGHTING_FACT_MODE_STARTED}, {23,1,NOAH_LIGHTING_FACT_MODE_ENDED}, {24,1,NOAH_LIGHTING_FACT_COMBO_FIRED},
        {25,2,NOAH_LIGHTING_FACT_TAP_FIRED}, {26,1,NOAH_LIGHTING_FACT_HOLD_FIRED}, {27,1,NOAH_LIGHTING_FACT_LONG_HOLD_FIRED}, {28,1,NOAH_LIGHTING_FACT_MACRO_PLAYED},
    };
    for (unsigned i = 0u; i < sizeof events / sizeof events[0]; i++) {
        assert(!!(limits.facts & (UINT32_C(1) << events[i].id)) == events[i].enabled);
        uint8_t bytes[24] = {5,0,58,0,0,0,0,1,0,0,255};
        unsigned at = 11u;
        bytes[at++] = events[i].parameters;
        bytes[at++] = events[i].id;
        bytes[at++] = 255u;
        if (events[i].parameters == 2u) bytes[at++] = 255u;
        bytes[at++] = 0x40; bytes[at++] = 0; // whole board
        bytes[at++] = 0x60; bytes[at++] = 0; // clear
        bytes[at++] = 0xa2; bytes[at++] = 3; bytes[at++] = 1; bytes[at++] = 0; // flash1
        bytes[8] = at - 9u;
        bytes[at++] = 0;
        check(bytes,at,&limits,events[i].enabled && NOAH_LIGHTING_TIMING_FLASH);
    }
    assert(!!(limits.timings & (1u << 2)) == !!NOAH_LIGHTING_TIMING_LINGER);
    assert(!!(limits.timings & (1u << 3)) == !!NOAH_LIGHTING_TIMING_FLASH);
    puts("Painting module capability/validation gates passed");
}
