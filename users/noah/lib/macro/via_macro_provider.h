// ────────────────────────────────────────────────────────────────────────────
// VIA Macro Provider
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef VIA_ENABLE
bool via_macro_provider_try_play(uint16_t action);
void via_macro_provider_invalidate_all(void);
// Call before VIA's stored macros change, in the same step as the write.
void via_macro_provider_storage_changing(void);
#else
static inline bool via_macro_provider_try_play(uint16_t action) {
    (void)action;
    return false;
}

static inline void via_macro_provider_invalidate_all(void) {}
static inline void via_macro_provider_storage_changing(void) {}
#endif
