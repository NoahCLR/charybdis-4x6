// ────────────────────────────────────────────────────────────────────────────
// Runtime Init
// ────────────────────────────────────────────────────────────────────────────
//
// Shared userspace init and scan orchestration. Owns the noah_* entry points
// called by hooks.c and coordinates the smaller runtime modules that seed
// defaults, validate authored data, scan the key engine, and initialize split
// and RGB state.
// ────────────────────────────────────────────────────────────────────────────

#include "noah_runtime.h"

#include "lib/action/macro_dispatch.h"
#include "lib/key/key_runtime_state.h"
#include "lib/key/keymap_validation.h"
#include "lib/macro/via_macro_defaults.h"
#include "lib/rgb/rgb_runtime.h"
#include "lib/state/runtime_shared_state.h"

void noah_eeconfig_init_user(void) {
#if (EECONFIG_USER_DATA_SIZE) == 0
    eeconfig_update_user(0);
#endif

    noah_via_macro_defaults_eeconfig_init();
}

void noah_matrix_scan_user(void) {
    noah_via_macro_defaults_matrix_scan();
    noah_key_runtime_scan();
    runtime_shared_state_sync_tick();
}

void noah_keyboard_post_init_user(void) {
    macro_dispatch_validate_all();
    noah_keymap_validate();
    noah_via_macro_defaults_keyboard_post_init();
    noah_rgb_runtime_post_init();
    runtime_shared_state_init();
}
