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

#include "qmk_tapping_contract.h"

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

static inline uint16_t noah_qmk_contract_auto_mouse_elapsed_at(uint32_t now) {
    return auto_mouse_get_time_elapsed_at((uint16_t)now);
}

static inline bool noah_qmk_contract_auto_mouse_active(void) {
    return is_auto_mouse_active();
}

// Whether trackball movement may activate the auto-mouse layer at all. QMK
// zeroes the activity timer when this turns off, so elapsed time is only
// meaningful while it is on.
static inline bool noah_qmk_contract_auto_mouse_enabled(void) {
    return get_auto_mouse_enable();
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

// What process_auto_mouse does for a non-mouse key record while auto-mouse is
// not active: on a press, turn its layer off and clear its state; on either
// edge, restart the delay before movement can bring the layer back.
static inline void noah_qmk_contract_auto_mouse_reset_trigger(bool pressed) {
    auto_mouse_reset_trigger(pressed);
}

// The layer QMK resolved this record's keycode from. process_record_quantum()
// stores it in the source-layer cache before any process_* hook runs, and a
// release reads the entry its press stored.
#    if defined(STRICT_LAYER_RELEASE) || defined(NO_ACTION_LAYER)
#        error "auto-mouse pointer policy needs QMK's source-layer cache"
#    endif
static inline uint8_t noah_qmk_contract_record_source_layer(const keyrecord_t *record) {
    return read_source_layers_cache(record->event.key);
}

// Whether QMK's process_auto_mouse flipped its own toggle for this record. It
// runs before process_record_kb and flips it on the release of TG() or TO() of
// its layer, and on the release of TT()'s TAPPING_TOGGLE-th tap of it.
static inline bool noah_qmk_contract_auto_mouse_record_toggles(uint16_t keycode, const keyrecord_t *record) {
    if (!record || record->event.pressed || !get_auto_mouse_enable()) {
        return false;
    }

    uint8_t layer = get_auto_mouse_layer();

    if (IS_QK_TO(keycode)) {
        return QK_TO_GET_LAYER(keycode) == layer;
    }
    if (IS_QK_TOGGLE_LAYER(keycode)) {
        return QK_TOGGLE_LAYER_GET_LAYER(keycode) == layer;
    }
#    if !defined(NO_ACTION_TAPPING) && NOAH_QMK_TAPPING_TOGGLE != 0
    if (IS_QK_LAYER_TAP_TOGGLE(keycode)) {
        return QK_LAYER_TAP_TOGGLE_GET_LAYER(keycode) == layer && record->tap.count == NOAH_QMK_TAPPING_TOGGLE;
    }
#    endif

    return false;
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

static inline uint16_t noah_qmk_contract_auto_mouse_elapsed_at(uint32_t now) {
    (void)now;
    return 0;
}

static inline bool noah_qmk_contract_auto_mouse_active(void) {
    return false;
}

static inline bool noah_qmk_contract_auto_mouse_enabled(void) {
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

static inline bool noah_qmk_contract_auto_mouse_record_toggles(uint16_t keycode, const keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}
#endif
