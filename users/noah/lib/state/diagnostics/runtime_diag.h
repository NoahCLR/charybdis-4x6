// ────────────────────────────────────────────────────────────────────────────
// Runtime Restart Watchdog
// ────────────────────────────────────────────────────────────────────────────
//
// Lightweight boot indicator and restart watchdog surface. The public scope
// API remains available so hot-path callers do not need conditional code, but
// normal builds do not write watchdog scratch state or report reboot stages.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NOAH_RUNTIME_DIAG_STAGE_IDLE = 0,
    NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD,
    NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD_FINALIZE,
    NOAH_RUNTIME_DIAG_STAGE_LAYER_STATE_SET,
    NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK,
    NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_VIA_DEFAULTS,
    NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_KEY_RUNTIME,
    NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN_SPLIT_SYNC,
    NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER,
    NOAH_RUNTIME_DIAG_STAGE_HOUSEKEEPING,
    NOAH_RUNTIME_DIAG_STAGE_POST_INIT,
} noah_runtime_diag_stage_t;

void                      noah_runtime_diag_post_init(void);
void                      noah_runtime_diag_scope_enter(noah_runtime_diag_stage_t stage);
void                      noah_runtime_diag_scope_leave(void);
void                      noah_runtime_diag_heartbeat(void);
noah_runtime_diag_stage_t noah_runtime_diag_current_stage(void);
bool                      noah_runtime_diag_watchdog_reboot_latched(void);
noah_runtime_diag_stage_t noah_runtime_diag_watchdog_stage(void);
uint8_t                   noah_runtime_diag_watchdog_reboot_count(void);
bool                      noah_runtime_diag_indicator_active(void);
void                      noah_runtime_diag_reset_for_test(void);

#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
void     noah_runtime_diag_test_backend_reset(void);
uint32_t noah_runtime_diag_test_backend_scratch(uint8_t index);
bool     noah_runtime_diag_test_backend_watchdog_enabled(void);
uint32_t noah_runtime_diag_test_backend_watchdog_enable_count(void);
uint32_t noah_runtime_diag_test_backend_watchdog_update_count(void);
#endif
