// ────────────────────────────────────────────────────────────────────────────
// VIA Macro Defaults
// ────────────────────────────────────────────────────────────────────────────
//
// Owns validation and seeding for authored default VIA macro payloads.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef VIA_ENABLE
void noah_via_macro_defaults_eeconfig_init(void);
void noah_via_macro_defaults_matrix_scan(void);
void noah_via_macro_defaults_keyboard_post_init(void);
bool noah_via_macro_defaults_last_seed_succeeded(void);
bool noah_via_macro_defaults_reseed_for_recovery(void);
#else
static inline void noah_via_macro_defaults_eeconfig_init(void) {}

static inline void noah_via_macro_defaults_matrix_scan(void) {}

static inline void noah_via_macro_defaults_keyboard_post_init(void) {}

static inline bool noah_via_macro_defaults_last_seed_succeeded(void) {
    return true;
}

static inline bool noah_via_macro_defaults_reseed_for_recovery(void) {
    return true;
}
#endif
