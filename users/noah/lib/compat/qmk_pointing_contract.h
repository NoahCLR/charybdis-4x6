// ────────────────────────────────────────────────────────────────────────────
// QMK Pointing Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Centralizes this userspace's dependence on the current Charybdis fork's
// pointer DPI and sniping APIs.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#if defined(POINTING_DEVICE_ENABLE)
#    ifndef CHARYBDIS_DRAGSCROLL_DPI
#        define CHARYBDIS_DRAGSCROLL_DPI 100
#    endif

static inline bool noah_qmk_contract_pointer_sniping_enabled(void) {
    return charybdis_get_pointer_sniping_enabled();
}

static inline void noah_qmk_contract_pointer_set_sniping_enabled(bool enable) {
    charybdis_set_pointer_sniping_enabled(enable);
}

static inline uint16_t noah_qmk_contract_pointer_default_dpi(void) {
    return charybdis_get_pointer_default_dpi();
}

static inline uint16_t noah_qmk_contract_pointer_dragscroll_dpi(void) {
    return CHARYBDIS_DRAGSCROLL_DPI;
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

static inline uint16_t noah_qmk_contract_pointer_dragscroll_dpi(void) {
    return 0;
}
#endif
