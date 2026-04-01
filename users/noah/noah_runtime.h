// ────────────────────────────────────────────────────────────────────────────
// Noah Runtime Entry Points
// ────────────────────────────────────────────────────────────────────────────
//
// Shared userspace hook implementations. users/noah/noah.c exposes the QMK
// *_user hooks and forwards to these helpers before the optional weak
// *_keymap delegates in keymap.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

bool           noah_get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record);
bool           noah_process_record_user(uint16_t keycode, keyrecord_t *record);
void           noah_matrix_scan_user(void);
void           noah_keyboard_post_init_user(void);
layer_state_t  noah_layer_state_set_user(layer_state_t state);
report_mouse_t noah_pointing_device_task_user(report_mouse_t mouse_report);
void           noah_pointing_device_init_user(void);
bool           noah_is_mouse_record_user(uint16_t keycode, keyrecord_t *record);
bool           noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max);
