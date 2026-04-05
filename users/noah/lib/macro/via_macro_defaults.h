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
#else
static inline void noah_via_macro_defaults_eeconfig_init(void) {
}

static inline void noah_via_macro_defaults_matrix_scan(void) {
}

static inline void noah_via_macro_defaults_keyboard_post_init(void) {
}
#endif
