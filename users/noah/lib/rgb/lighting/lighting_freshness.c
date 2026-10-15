#include "lighting_freshness.h"
#include <limits.h>

void noah_lighting_freshness_reset(noah_lighting_freshness_t *clock) {
    if (clock) *clock = (noah_lighting_freshness_t){0};
}
bool noah_lighting_freshness_calibrate(noah_lighting_freshness_t *clock, uint32_t capture, uint32_t before, uint32_t remote, uint32_t after) {
    if (!clock) return false;
    noah_lighting_freshness_reset(clock);
    uint32_t assembly = before - capture, rtt = after - before;
    if (assembly > INT32_MAX || rtt > INT32_MAX || assembly > (uint32_t)INT32_MAX - rtt) return false;
    *clock = (noah_lighting_freshness_t){capture, before, after, remote, true};
    return true;
}
static uint16_t saturate(uint32_t age) {
    return age >= UINT16_MAX ? UINT16_MAX : (uint16_t)age;
}
bool noah_lighting_freshness_age(const noah_lighting_freshness_t *clock, uint32_t now, uint16_t *lower, uint16_t *upper) {
    if (!clock || !clock->valid || !lower || !upper) return false;
    uint32_t elapsed = now - clock->remote, minimum = clock->before - clock->capture, maximum = clock->after - clock->capture;
    if (elapsed > INT32_MAX || maximum > (uint32_t)INT32_MAX - elapsed) return false;
    minimum += elapsed;
    maximum += elapsed;
    // Causal callback lies between the request's source endpoints. Each of
    // source-endpoint minus capture and receiver-now minus callback subtracts
    // two floor-to-ms readings: each differs from real elapsed by <1 ms.
    // Their sum therefore differs by <2 ms. Outward integer +/-2 is safe;
    // neither a symmetric-delay assumption nor an invented tick allowance.
    *lower = saturate(minimum > 2u ? minimum - 2u : 0u);
    *upper = saturate(maximum + 2u);
    return true;
}
