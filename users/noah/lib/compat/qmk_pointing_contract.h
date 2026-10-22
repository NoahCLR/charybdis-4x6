// ────────────────────────────────────────────────────────────────────────────
// QMK Pointing Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Centralizes this userspace's dependence on the current Charybdis fork's
// pointer DPI and sniping APIs, and on when QMK sends a mouse report.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#if defined(POINTING_DEVICE_ENABLE)
static inline bool noah_qmk_contract_pointer_sniping_enabled(void) {
    return charybdis_get_pointer_sniping_enabled();
}

static inline void noah_qmk_contract_pointer_set_sniping_enabled(bool enable) {
    charybdis_set_pointer_sniping_enabled(enable);
}

static inline uint16_t noah_qmk_contract_pointer_default_dpi(void) {
    return charybdis_get_pointer_default_dpi();
}

static inline uint16_t noah_qmk_contract_pointer_sniping_dpi(void) {
    return charybdis_get_pointer_sniping_dpi();
}

// QMK sends a mouse button's report at once, from the report the pointing task
// is still building, which carries this poll's motion until the task's own
// result replaces it. A pointing mode that clicks drops that motion first, so
// the click cannot move a cursor the mode holds still. Outside the pointing
// task the report has no motion and this changes nothing.
static inline void noah_qmk_contract_pointer_drop_pending_motion(void) {
    report_mouse_t report = pointing_device_get_report();

    report.x = 0;
    report.y = 0;
    report.h = 0;
    report.v = 0;
    pointing_device_set_report(report);
}
#else
static inline bool noah_qmk_contract_pointer_sniping_enabled(void) {
    return false;
}

static inline void noah_qmk_contract_pointer_set_sniping_enabled(bool enable) {
    (void)enable;
}

static inline uint16_t noah_qmk_contract_pointer_default_dpi(void) {
    return 0;
}

static inline uint16_t noah_qmk_contract_pointer_sniping_dpi(void) {
    return 0;
}

static inline void noah_qmk_contract_pointer_drop_pending_motion(void) {}
#endif
