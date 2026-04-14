// ────────────────────────────────────────────────────────────────────────────
// VIA Macro Provider
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef VIA_ENABLE
bool via_macro_provider_try_play(uint16_t action);
void via_macro_provider_invalidate_all(void);
#else
static inline bool via_macro_provider_try_play(uint16_t action) {
    (void)action;
    return false;
}

static inline void via_macro_provider_invalidate_all(void) {}
#endif
