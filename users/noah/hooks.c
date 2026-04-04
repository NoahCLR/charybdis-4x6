#include "noah_runtime.h"

// Weak default QMK hooks for the noah userspace.
// Any keymap-local override can define the normal QMK *_user hook and call
// the matching noah_* helper if it still wants the shared userspace behavior.

__attribute__((weak)) void eeconfig_init_user(void) {
    noah_eeconfig_init_user();
}

__attribute__((weak)) bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    return noah_get_hold_on_other_key_press(keycode, record);
}

__attribute__((weak)) bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    return noah_process_record_user(keycode, record);
}

__attribute__((weak)) void matrix_scan_user(void) {
    noah_matrix_scan_user();
}

__attribute__((weak)) void keyboard_post_init_user(void) {
    noah_keyboard_post_init_user();
}

__attribute__((weak)) layer_state_t layer_state_set_user(layer_state_t state) {
    return noah_layer_state_set_user(state);
}

__attribute__((weak)) report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    return noah_pointing_device_task_user(mouse_report);
}

__attribute__((weak)) void pointing_device_init_user(void) {
    noah_pointing_device_init_user();
}

__attribute__((weak)) bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    return noah_is_mouse_record_user(keycode, record);
}

__attribute__((weak)) bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    return noah_rgb_matrix_indicators_advanced_user(led_min, led_max);
}
