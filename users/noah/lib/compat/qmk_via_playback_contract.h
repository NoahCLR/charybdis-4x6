// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Playback Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Centralizes this userspace's dependence on the current fork's VIA dynamic
// macro playback encoding.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#ifdef VIA_ENABLE
uint8_t  noah_qmk_via_macro_count(void);
uint16_t noah_qmk_via_macro_buffer_size(void);
void     noah_qmk_via_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data);
#else
static inline uint8_t noah_qmk_via_macro_count(void) {
    return 0;
}

static inline uint16_t noah_qmk_via_macro_buffer_size(void) {
    return 0;
}

static inline void noah_qmk_via_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    (void)offset;
    (void)size;
    (void)data;
}
#endif

bool noah_qmk_contract_try_play_via_macro(uint16_t action);
