// ────────────────────────────────────────────────────────────────────────────
// PD Mode Handlers
// ────────────────────────────────────────────────────────────────────────────
//
// Public declarations for the per-mode motion handlers.
// Implementations live in the per-mode translation units under this folder.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

report_mouse_t handle_volume_mode(report_mouse_t mouse_report);
report_mouse_t handle_brightness_mode(report_mouse_t mouse_report);
report_mouse_t handle_zoom_mode(report_mouse_t mouse_report);
report_mouse_t handle_arrow_mode(report_mouse_t mouse_report);
report_mouse_t handle_dragscroll_mode(report_mouse_t mouse_report);

bool handle_arrow_mode_key(uint16_t keycode, keyrecord_t *record);

void reset_dragscroll_mode(void);
void reset_volume_mode(void);
void reset_brightness_mode(void);
void reset_zoom_mode(void);
void reset_arrow_mode(void);
