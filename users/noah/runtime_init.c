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
#include "lib/compat/qmk_record_admission.h"
#include "lib/compat/qmk_durable_io.h"
#include "lib/compat/qmk_loop_stages.h"
#include "lib/compat/qmk_via_split_mirror.h"
#include "lib/compat/qmk_via_split_sync.h"
#include "lib/compat/qmk_via_sync_state.h"
#include "lib/profile/storage/profile_store_runtime_hooks.h"
#include "lib/profile/runtime/effective_pd_runtime.h"
#include "lib/rgb/core/rgb_runtime.h"
#include "lib/state/diagnostics/runtime_diag.h"
#include "lib/state/shared/runtime_reset.h"
#include "lib/split/runtime_sync.h"

typedef void (*noah_runtime_init_stage_fn_t)(void);

static void noah_runtime_init_finalize_via_sync_defaults(void) {
    noah_qmk_via_sync_state_reset_after_defaults(noah_via_macro_defaults_last_seed_succeeded());
}

#if defined(NOAH_PD_PROFILE_ENABLE) && !defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
static void noah_runtime_init_compiled_pd(void) {
    noah_effective_pd_load_compiled_defaults(noah_pd_defaults);
}
#endif

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
#if defined(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE)
    noah_runtime_cadence_note_matrix_scan();
#endif
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_DURABLE_IO);
    noah_via_macro_defaults_matrix_scan();
    noah_qmk_durable_io_matrix_scan();

    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_KEY_RUNTIME);
    noah_qmk_combo_origin_scan();

    noah_key_runtime_scan();
    noah_record_admission_task();

    macro_payload_engine_scan();

    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_SPLIT_SYNC);
    split_runtime_sync_tick();
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_QMK_TASKS);
}

void noah_matrix_slave_scan_user(void) {
    // QMK does not call matrix_scan_user() on the slave half. Keep the
    // durable receiver scheduler alive there without running master-side key,
    // macro, or shared-state senders a second time.
    noah_qmk_durable_io_matrix_scan();
}

void noah_housekeeping_task_user(void) {
    held_repeat_tick();
    noah_runtime_diag_heartbeat();
}

void noah_suspend_power_down_user(void) {
    // While the host has USB suspended, QMK loops inside protocol_pre_task()
    // and never reaches housekeeping_task(), so the restart watchdog would
    // reset the master about 2 s after the host sleeps. The suspend loop
    // calls this every pass (~17 ms), so a hang there still trips the watchdog.
    noah_runtime_diag_heartbeat();
}

void noah_keyboard_post_init_user(void) {
    static const noah_runtime_init_stage_fn_t stages[] = {
        noah_runtime_shared_state_post_init, key_origin_registry_init, noah_qmk_combo_origin_init, noah_record_admission_reset, macro_payload_engine_init, noah_via_macro_defaults_keyboard_post_init, noah_profile_store_runtime_init,
#if defined(NOAH_PD_PROFILE_ENABLE) && !defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
        noah_runtime_init_compiled_pd,
#endif
        noah_rgb_runtime_post_init, split_runtime_sync_init, noah_qmk_via_split_sync_init, noah_qmk_via_split_mirror_init, noah_qmk_durable_io_init,
#if defined(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE)
        noah_qmk_loop_stages_init,
#endif
    };

    noah_runtime_diag_post_init();

    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
}
