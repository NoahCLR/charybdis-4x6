// ────────────────────────────────────────────────────────────────────────────
// QMK Auto-Mouse Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Centralizes this userspace's dependence on the current firmware fork's
// pointing_device_auto_mouse APIs.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
#    include "pointing_device_auto_mouse.h" // QMK (firmware fork)

static inline bool noah_qmk_contract_auto_mouse_toggle_enabled(void) {
    return get_auto_mouse_toggle();
}

static inline int8_t noah_qmk_contract_auto_mouse_key_tracker(void) {
    return get_auto_mouse_key_tracker();
}

static inline uint8_t noah_qmk_contract_auto_mouse_layer(void) {
    return get_auto_mouse_layer();
}

static inline uint16_t noah_qmk_contract_auto_mouse_elapsed(void) {
    return auto_mouse_get_time_elapsed();
}

static inline bool noah_qmk_contract_auto_mouse_active(void) {
    return is_auto_mouse_active();
}

static inline void noah_qmk_contract_auto_mouse_set_enable(bool enable) {
    set_auto_mouse_enable(enable);
}

static inline void noah_qmk_contract_auto_mouse_set_layer(uint8_t layer) {
    set_auto_mouse_layer(layer);
}

static inline void noah_qmk_contract_auto_mouse_layer_off(void) {
    auto_mouse_layer_off();
}

static inline void noah_qmk_contract_auto_mouse_toggle(void) {
    auto_mouse_toggle();
}

static inline void noah_qmk_contract_auto_mouse_keyevent(bool pressed) {
    auto_mouse_keyevent(pressed);
}
#else
static inline bool noah_qmk_contract_auto_mouse_toggle_enabled(void) {
    return false;
}

static inline int8_t noah_qmk_contract_auto_mouse_key_tracker(void) {
    return 0;
}

static inline uint8_t noah_qmk_contract_auto_mouse_layer(void) {
    return 0;
}

static inline uint16_t noah_qmk_contract_auto_mouse_elapsed(void) {
    return 0;
}

static inline bool noah_qmk_contract_auto_mouse_active(void) {
    return false;
}

static inline void noah_qmk_contract_auto_mouse_set_enable(bool enable) {
    (void)enable;
}

static inline void noah_qmk_contract_auto_mouse_set_layer(uint8_t layer) {
    (void)layer;
}

static inline void noah_qmk_contract_auto_mouse_layer_off(void) {}

static inline void noah_qmk_contract_auto_mouse_toggle(void) {}

static inline void noah_qmk_contract_auto_mouse_keyevent(bool pressed) {
    (void)pressed;
}
#endif
