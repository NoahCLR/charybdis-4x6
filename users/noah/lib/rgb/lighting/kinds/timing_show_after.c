// Timing 1, show after: hidden until the subject has existed for the delay.
#include "../lighting.h"

#if NOAH_LIGHTING_TIMING_SHOW_AFTER

uint8_t noah_lighting_timing_show_after(const uint8_t *params, uint32_t elapsed, uint32_t *next_change) {
    uint32_t delay = (uint32_t)params[0] | (uint32_t)params[1] << 8u;

    if (elapsed >= delay) return UINT8_MAX;
    *next_change = delay - elapsed;
    return 0u;
}

#endif
