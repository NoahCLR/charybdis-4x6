// ──────────────────────────────────────────────────────────────────────────
// QMK Loop Stage Boundaries
// ──────────────────────────────────────────────────────────────────────────
//
// Performance-diagnostics builds only. The cadence recorder's stage timing
// needs three boundaries no userspace hook sees: keyboard_task's entry and
// return, and the pointing driver's report read. QMK's main loop calls
// keyboard_task through the weak protocol_keyboard_task, which this file
// replaces; the sensor read is timed by pointing QMK's driver table pointer
// (pointing_device.c, not in a header) at a copy with a timed get_report.
// Both are upstream QMK, not fork, contracts; losing either symbol fails the
// link. The noah_* hooks mark every other stage.
// ──────────────────────────────────────────────────────────────────────────

#include "qmk_loop_stages.h"

#ifdef NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE
#    include QMK_KEYBOARD_H // IWYU pragma: keep

#    include "lib/state/diagnostics/runtime_diag.h"

void protocol_keyboard_task(void); // main.c's weak default has no header

void protocol_keyboard_task(void) {
    noah_runtime_cadence_loop_begin();
    keyboard_task();
    noah_runtime_diag_stage_mark(NOAH_RUNTIME_DIAG_STAGE_OUTSIDE_KEYBOARD_TASK);
}

#    ifdef POINTING_DEVICE_ENABLE
extern const pointing_device_driver_t *pointing_device_driver;

static pointing_device_driver_t noah_qmk_loop_stages_timed_driver;
static report_mouse_t (*noah_qmk_loop_stages_untimed_get_report)(report_mouse_t mouse_report);

static report_mouse_t noah_qmk_loop_stages_timed_get_report(report_mouse_t mouse_report) {
    noah_runtime_diag_scope_enter(NOAH_RUNTIME_DIAG_STAGE_SENSOR_READ);
    mouse_report = noah_qmk_loop_stages_untimed_get_report(mouse_report);
    noah_runtime_diag_scope_leave();
    return mouse_report;
}
#    endif

// keyboard_init has run pointing_device_init by keyboard_post_init.
void noah_qmk_loop_stages_init(void) {
#    ifdef POINTING_DEVICE_ENABLE
    if (!pointing_device_driver || pointing_device_driver == &noah_qmk_loop_stages_timed_driver) {
        return;
    }
    noah_qmk_loop_stages_timed_driver            = *pointing_device_driver;
    noah_qmk_loop_stages_untimed_get_report      = noah_qmk_loop_stages_timed_driver.get_report;
    noah_qmk_loop_stages_timed_driver.get_report = noah_qmk_loop_stages_timed_get_report;
    pointing_device_driver                       = &noah_qmk_loop_stages_timed_driver;
#    endif
}
#endif
