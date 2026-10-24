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
#include "lib/compat/qmk_combo_origin.h"
#include "lib/compat/qmk_record_admission.h"
#include "lib/compat/qmk_keyboard_restart.h"
#include "lib/compat/qmk_loop_stages.h"
#include "lib/compat/qmk_profile_flash_hooks.h"
#include "lib/compat/qmk_portable_profile.h"
#include "lib/compat/qmk_usb_product_name.h"
#include "lib/profile/storage/profile_store_runtime_hooks.h"
#include "lib/profile/runtime/active_profile_pd_runtime.h"
#include "lib/rgb/core/rgb_runtime.h"
#include "lib/state/diagnostics/runtime_diag.h"
#include "lib/state/shared/runtime_reset.h"
#include "lib/split/runtime_sync.h"

typedef void (*noah_runtime_init_stage_fn_t)(void);

#if !defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
static void noah_runtime_init_compiled_pd(void) {
    noah_active_profile_pd_load_compiled_defaults(noah_pd_defaults);
}
#endif

void noah_eeconfig_init_user(void) {
    // An EEPROM reset forgets the saved profiles at boot (D-F29); nothing
    // else of ours lives in QMK's EEPROM (D-F31).
    (void)macro_payload_engine_cancel();
}

void noah_matrix_scan_user(void) {
#if defined(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE)
    noah_runtime_cadence_note_matrix_scan();
#endif
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_DURABLE_IO);
    noah_profile_store_runtime_matrix_scan();

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
    // QMK does not call matrix_scan_user() on the slave half. Keep its
    // durable-I/O grant running there without running master-side key,
    // macro, or shared-state senders a second time.
    noah_profile_store_runtime_matrix_scan();
}

void noah_housekeeping_task_user(void) {
    held_repeat_tick();
    noah_runtime_diag_heartbeat();
    noah_qmk_usb_product_name_task();
    noah_qmk_profile_flash_housekeeping();
    // Last: a restart a client asked for runs between main-loop passes, never
    // inside a flash step (D-F34).
    noah_qmk_keyboard_restart_task();
}

void noah_suspend_power_down_user(void) {
    // While the host has USB suspended, QMK loops inside protocol_pre_task()
    // and never reaches housekeeping_task(), so the restart watchdog would
    // reset the master about 2 s after the host sleeps. The suspend loop
    // calls this every pass (~17 ms), so a hang there still trips the watchdog.
    noah_runtime_diag_heartbeat();
}

// Before USB starts: the logical EEPROM is readable, nothing else is up yet.
// An EEPROM reset forgets the saved profiles before the name is read from
// them.
void noah_keyboard_pre_init_user(void) {
    noah_qmk_profile_flash_pre_init();
    noah_qmk_usb_product_name_pre_init();
}

void noah_keyboard_post_init_user(void) {
    static const noah_runtime_init_stage_fn_t stages[] = {
        noah_runtime_shared_state_post_init, key_origin_registry_init, noah_qmk_combo_origin_init, noah_record_admission_reset, macro_payload_engine_init, noah_qmk_portable_storage_init, noah_profile_store_runtime_init,
#if !defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
        noah_runtime_init_compiled_pd,
#endif
        noah_rgb_runtime_post_init, split_runtime_sync_init,
#if defined(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE)
        noah_qmk_loop_stages_init,
#endif
    };

    noah_runtime_diag_post_init();

    for (uint8_t index = 0; index < ARRAY_SIZE(stages); index++) {
        stages[index]();
    }
}
