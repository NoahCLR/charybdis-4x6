// ────────────────────────────────────────────────────────────────────────────
// Runtime Init
// ────────────────────────────────────────────────────────────────────────────
//
// Shared userspace init and scan orchestration. Owns the noah_* entry points
// called by hooks.c and coordinates the smaller runtime modules that seed
// defaults, scan the key engine, and initialize split runtime sync and RGB
// state.
// ────────────────────────────────────────────────────────────────────────────

#include "noah_runtime.h"

#include <stdint.h>

#include "lib/key/ownership/held_repeat.h"
#include "lib/key/runtime/api.h"
#include "lib/key/runtime/slot/origin_registry.h"
#include "lib/macro/macro_payload.h"
#include "lib/macro/via_macro_defaults.h"
#include "lib/compat/qmk_combo_origin.h"
#include "lib/compat/qmk_via_split_sync.h"
#include "lib/compat/qmk_via_sync_state.h"
#include "lib/rgb/core/rgb_runtime.h"
#include "lib/state/diagnostics/runtime_diag.h"
#include "lib/split/runtime_sync.h"

typedef void (*noah_runtime_init_stage_fn_t)(void);

static void noah_runtime_init_finalize_via_sync_defaults(void) {
    noah_qmk_via_sync_state_reset_after_defaults(noah_via_macro_defaults_last_seed_succeeded());
}

void noah_eeconfig_init_user(void) {
    static const noah_runtime_init_stage_fn_t stages[] = {
        noah_via_macro_defaults_eeconfig_init,
        noah_runtime_init_finalize_via_sync_defaults,
    };

    (void)macro_payload_engine_cancel();
    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
}

void noah_matrix_scan_user(void) {
    noah_via_macro_defaults_matrix_scan();
    noah_qmk_via_split_sync_matrix_scan();

    noah_qmk_combo_origin_scan();

    noah_key_runtime_scan();

    macro_payload_engine_scan();

    split_runtime_sync_tick();
}

void noah_housekeeping_task_user(void) {
    held_repeat_tick();
    noah_runtime_diag_heartbeat();
}

void noah_keyboard_post_init_user(void) {
    static const noah_runtime_init_stage_fn_t stages[] = {
        key_origin_registry_init, noah_qmk_combo_origin_init, macro_payload_engine_init, noah_via_macro_defaults_keyboard_post_init, noah_rgb_runtime_post_init, split_runtime_sync_init, noah_qmk_via_split_sync_init,
    };

    noah_runtime_diag_post_init();

    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
}
