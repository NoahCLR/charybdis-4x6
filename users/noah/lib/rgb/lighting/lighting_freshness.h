#pragma once
#include <stdbool.h>
#include <stdint.h>

// Nominal equal-rate logical milliseconds, arbitrary offsets. This does not
// assert a physical oscillator tolerance. All evidence/use spans must be below
// half the u32 range, and resets/role/profile changes invalidate the evidence.
typedef struct {
    uint32_t capture, before, after, remote;
    bool     valid;
} noah_lighting_freshness_t;
void noah_lighting_freshness_reset(noah_lighting_freshness_t *);
bool noah_lighting_freshness_calibrate(noah_lighting_freshness_t *, uint32_t capture, uint32_t before, uint32_t remote, uint32_t after);
// Bounds elapsed source logical milliseconds since snapshot capture. False is
// unknown; callers suppress transient paint. A 65535 bound is saturated/expired,
// never wrapped. This includes assembly, transfer, main consume and later time.
bool noah_lighting_freshness_age(const noah_lighting_freshness_t *, uint32_t now, uint16_t *lower, uint16_t *upper);
