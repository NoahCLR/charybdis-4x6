// ────────────────────────────────────────────────────────────────────────────
// Runtime Restart Watchdog
// ────────────────────────────────────────────────────────────────────────────
//
// Lightweight boot indicator and restart watchdog surface. The public scope
// API remains available so hot-path callers do not need conditional code, but
// normal builds do not write watchdog scratch state or report reboot stages.
// Performance-diagnostics builds add the cadence recorder, which uses the
// scopes and stage marks to time each loop stage.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Stages the cadence recorder times, in wire order (stage N is wire slot
// N - 1). Time is exclusive: a nested scope's time is not also charged to the
// stage it interrupts. IDLE is never timed; the watchdog APIs report it.
typedef enum {
    NOAH_RUNTIME_DIAG_STAGE_IDLE = 0,
    // keyboard_task entry to matrix_scan_user: local matrix, debounce and
    // QMK's split transport.
    NOAH_RUNTIME_DIAG_STAGE_MATRIX_SCAN,
    // VIA macro defaults and the durable I/O scan (profile and VIA sync RPCs).
    NOAH_RUNTIME_DIAG_STAGE_DURABLE_IO,
    // Combo origins, the key runtime scan and the macro engine.
    NOAH_RUNTIME_DIAG_STAGE_KEY_RUNTIME,
    // Runtime split sync RPCs.
    NOAH_RUNTIME_DIAG_STAGE_SPLIT_SYNC,
    // matrix_scan_user's return to the pointing hook: key event dispatch,
    // quantum_task, rgb_matrix_task and the keyboard's pointing code.
    NOAH_RUNTIME_DIAG_STAGE_QMK_TASKS,
    // process_record_user and its finalize, wherever they run.
    NOAH_RUNTIME_DIAG_STAGE_PROCESS_RECORD,
    // The lighting render in rgb_matrix_indicators_advanced_user.
    NOAH_RUNTIME_DIAG_STAGE_RGB_RENDER,
    // The pointing driver's report read.
    NOAH_RUNTIME_DIAG_STAGE_SENSOR_READ,
    // pointing_device_task_user.
    NOAH_RUNTIME_DIAG_STAGE_POINTING_TASK,
    // The pointing hook's return to keyboard_task's end: auto-mouse, the USB
    // mouse report, mousekey and LED tasks.
    NOAH_RUNTIME_DIAG_STAGE_POINTING_REPORT,
    // Between keyboard_task calls: USB events, raw HID and VIA, deferred
    // executors, housekeeping.
    NOAH_RUNTIME_DIAG_STAGE_OUTSIDE_KEYBOARD_TASK,
    NOAH_RUNTIME_DIAG_STAGE_COUNT,
} noah_runtime_diag_stage_t;

// Cadence recorder wire format 2, VIA custom get value 0x03 (Profile Wire
// channel), diagnostic builds only. Payloads are 25 bytes, little endian.
//
// Page 0, metadata: format, page count, completed windows, LE32 sequence
// (windows stored since boot), LE32 window length in us, five LE16 gap
// histogram upper bounds in us, started flag, timed stage count, stages per
// stage page, stage total unit as a power of two in us.
//
// Pages 1..WINDOW_COUNT, one per window, oldest first: LE32 sequence, window
// index (0xff when absent), LE32 longest pointing-poll gap, LE16 matrix scans,
// LE16 pointing polls, six LE16 gap histogram counts.
//
// Then STAGE_PAGES_PER_WINDOW pages per window, in window order: LE32
// sequence, window index, and for each of STAGES_PER_PAGE stages an LE16 total
// in stage units and an LE16 longest single-loop time in us, both saturating.
// Slots past the last stage are zero.
//
// A reader checks every page's sequence against page 0's: a window stored
// between them shifts the indices.
enum {
    NOAH_RUNTIME_CADENCE_WIRE_VALUE              = 0x03u,
    NOAH_RUNTIME_CADENCE_WIRE_FORMAT             = 2u,
    NOAH_RUNTIME_CADENCE_WINDOW_COUNT            = 30u,
    NOAH_RUNTIME_CADENCE_HISTOGRAM_BUCKETS       = 6u,
    NOAH_RUNTIME_CADENCE_TIMED_STAGES            = NOAH_RUNTIME_DIAG_STAGE_COUNT - 1u,
    NOAH_RUNTIME_CADENCE_STAGES_PER_PAGE         = 5u,
    NOAH_RUNTIME_CADENCE_STAGE_PAGES_PER_WINDOW  = (NOAH_RUNTIME_CADENCE_TIMED_STAGES + NOAH_RUNTIME_CADENCE_STAGES_PER_PAGE - 1u) / NOAH_RUNTIME_CADENCE_STAGES_PER_PAGE,
    NOAH_RUNTIME_CADENCE_STAGE_TOTAL_UNIT_SHIFT  = 4u,
    NOAH_RUNTIME_CADENCE_FIRST_STAGE_PAGE        = 1u + NOAH_RUNTIME_CADENCE_WINDOW_COUNT,
    NOAH_RUNTIME_CADENCE_WIRE_PAGES              = NOAH_RUNTIME_CADENCE_FIRST_STAGE_PAGE + NOAH_RUNTIME_CADENCE_WINDOW_COUNT * NOAH_RUNTIME_CADENCE_STAGE_PAGES_PER_WINDOW,
    NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE       = 25u,
};

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

#if defined(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE) || defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
void noah_runtime_cadence_note_matrix_scan(void);
void noah_runtime_cadence_note_pointing_poll(void);
// Called at keyboard_task entry: ends the previous loop, folds its stage times
// into the current window and starts timing MATRIX_SCAN.
void noah_runtime_cadence_loop_begin(void);
void noah_runtime_cadence_stage(noah_runtime_diag_stage_t stage);
bool noah_runtime_cadence_wire_page(uint8_t page, uint8_t payload[NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE]);
#endif

// Moves the loop to the next top-level stage and ends any open scopes.
// Compiles to nothing outside performance-diagnostics builds.
static inline void noah_runtime_diag_stage_mark(noah_runtime_diag_stage_t stage) {
#if defined(NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE) || defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
    noah_runtime_cadence_stage(stage);
#else
    (void)stage;
#endif
}

#if defined(NOAH_RUNTIME_DIAG_TEST_BACKEND)
void     noah_runtime_diag_test_backend_reset(void);
void     noah_runtime_diag_test_backend_set_realtime_counter(uint32_t value);
uint32_t noah_runtime_diag_test_backend_scratch(uint8_t index);
bool     noah_runtime_diag_test_backend_watchdog_enabled(void);
uint32_t noah_runtime_diag_test_backend_watchdog_enable_count(void);
uint32_t noah_runtime_diag_test_backend_watchdog_update_count(void);
#endif
