// ────────────────────────────────────────────────────────────────────────────
// QMK Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Centralizes this userspace's reliance on fork-specific or loosely-coupled
// QMK contracts:
//   - VIA dynamic macro playback encoding
//   - register_mods()/unregister_mods() symbol overrides
//   - pointing_device_auto_mouse.h APIs from the current firmware fork
//
// Runtime modules should depend on this compatibility surface rather than
// including those fork-specific headers directly.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

bool noah_qmk_contract_try_play_via_macro(uint16_t action);

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
