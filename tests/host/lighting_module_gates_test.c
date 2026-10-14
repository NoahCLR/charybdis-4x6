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
    puts("Painting module capability/validation gates passed");
}
