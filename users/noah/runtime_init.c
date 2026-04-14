// ────────────────────────────────────────────────────────────────────────────
// Runtime Init
// ────────────────────────────────────────────────────────────────────────────
//
// Shared userspace init and scan orchestration. Owns the noah_* entry points
// called by hooks.c and coordinates the smaller runtime modules that seed
// defaults, validate authored data, scan the key engine, and initialize split
// runtime sync and RGB state.
// ────────────────────────────────────────────────────────────────────────────

#include "noah_runtime.h"

#include "lib/key/ownership/held_repeat.h"
#include "lib/key/runtime/key_runtime_api.h"
#include "lib/key/interaction/keymap_validation.h"
#include "lib/macro/macro_dispatch.h"
#include "lib/macro/via_macro_defaults.h"
#include "lib/rgb/core/rgb_runtime.h"
#include "lib/state/runtime/split_runtime_sync.h"

typedef void (*noah_runtime_init_stage_fn_t)(void);

static void noah_runtime_init_seed_eeconfig_defaults(void) {
#if (EECONFIG_USER_DATA_SIZE) == 0
    eeconfig_update_user(0);
#endif
}

void noah_eeconfig_init_user(void) {
    static const noah_runtime_init_stage_fn_t stages[] = {
        noah_runtime_init_seed_eeconfig_defaults,
        noah_via_macro_defaults_eeconfig_init,
    };

    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
}

void noah_matrix_scan_user(void) {
    static const noah_runtime_init_stage_fn_t stages[] = {
        noah_via_macro_defaults_matrix_scan,
        noah_key_runtime_scan,
        split_runtime_sync_tick,
    };

    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
}

void noah_housekeeping_task_user(void) {
    static const noah_runtime_init_stage_fn_t stages[] = {
        held_repeat_tick,
    };

    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
}

void noah_keyboard_post_init_user(void) {
    static const noah_runtime_init_stage_fn_t stages[] = {
        macro_dispatch_validate_all, noah_keymap_validate, noah_via_macro_defaults_keyboard_post_init, noah_rgb_runtime_post_init, split_runtime_sync_init,
    };

    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
}
