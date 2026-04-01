#include QMK_KEYBOARD_H // QMK

#include "noah_runtime.h"

__attribute__((weak)) bool get_hold_on_other_key_press_keymap(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

__attribute__((weak)) bool process_record_keymap(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return true;
}

__attribute__((weak)) void matrix_scan_keymap(void) {
}

__attribute__((weak)) void keyboard_post_init_keymap(void) {
}

__attribute__((weak)) layer_state_t layer_state_set_keymap(layer_state_t state) {
    return state;
}

__attribute__((weak)) report_mouse_t pointing_device_task_keymap(report_mouse_t mouse_report) {
    return mouse_report;
}

__attribute__((weak)) void pointing_device_init_keymap(void) {
}

__attribute__((weak)) bool is_mouse_record_keymap(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

__attribute__((weak)) bool rgb_matrix_indicators_advanced_keymap(uint8_t led_min, uint8_t led_max) {
    (void)led_min;
    (void)led_max;
    return true;
}

bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    if (noah_get_hold_on_other_key_press(keycode, record)) {
        return true;
    }

    return get_hold_on_other_key_press_keymap(keycode, record);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!noah_process_record_user(keycode, record)) {
        return false;
    }

    return process_record_keymap(keycode, record);
}

void matrix_scan_user(void) {
    noah_matrix_scan_user();
    matrix_scan_keymap();
}

void keyboard_post_init_user(void) {
    noah_keyboard_post_init_user();
    keyboard_post_init_keymap();
}

layer_state_t layer_state_set_user(layer_state_t state) {
    state = noah_layer_state_set_user(state);
    return layer_state_set_keymap(state);
}

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    mouse_report = noah_pointing_device_task_user(mouse_report);
    return pointing_device_task_keymap(mouse_report);
}

void pointing_device_init_user(void) {
    noah_pointing_device_init_user();
    pointing_device_init_keymap();
}

bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    if (noah_is_mouse_record_user(keycode, record)) {
        return true;
    }

    return is_mouse_record_keymap(keycode, record);
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool continue_kb = noah_rgb_matrix_indicators_advanced_user(led_min, led_max);

    if (!rgb_matrix_indicators_advanced_keymap(led_min, led_max)) {
        return false;
    }

    return continue_kb;
}
