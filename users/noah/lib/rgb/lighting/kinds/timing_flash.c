#include "../lighting.h"
#include "../lighting_history_index.h"
#if NOAH_LIGHTING_TIMING_FLASH
// Elapsed is since qualifying occurrence, supplied by retained history.
uint8_t noah_lighting_timing_flash(const uint8_t *params, uint32_t elapsed, uint32_t *next_change) {
    uint16_t duration = (uint16_t)params[0] | (uint16_t)params[1] << 8u;
    *next_change      = elapsed < duration ? duration - elapsed : 0u;
    return elapsed < duration ? UINT8_MAX : 0u;
}
void noah_lighting_timing_flash_lifetime(const uint8_t *params, noah_lighting_lifetime_parameters_t *timing) {
    timing->flash = (uint16_t)params[0] | (uint16_t)params[1] << 8u;
}
#endif
