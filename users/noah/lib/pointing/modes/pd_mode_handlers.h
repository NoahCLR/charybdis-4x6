// Shared dragscroll backend used by configured pointing slots.
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

report_mouse_t handle_dragscroll_mode(report_mouse_t mouse_report);
void reset_dragscroll_mode(void);
