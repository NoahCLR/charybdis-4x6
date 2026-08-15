// ────────────────────────────────────────────────────────────────────────────
// PD Mode Handlers
// ────────────────────────────────────────────────────────────────────────────
//
// Public declarations for the per-mode motion handlers.
// Implementations live in the per-mode translation units under this folder.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

typedef struct {
    int32_t  accumulated_motion;
    uint32_t saturated_report_count;
    uint32_t discarded_tap_count;
    uint16_t pending_tap_count;
    uint16_t maximum_backlog_taps;
    uint8_t  maximum_taps_emitted;
    int8_t   direction;
} pd_mode_axis_debug_snapshot_t;

typedef struct {
    pd_mode_axis_debug_snapshot_t horizontal;
    pd_mode_axis_debug_snapshot_t vertical;
    bool                          selected_axis_is_horizontal;
} pd_mode_arrow_debug_snapshot_t;

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

void pd_mode_volume_debug_snapshot(pd_mode_axis_debug_snapshot_t *out);
void pd_mode_brightness_debug_snapshot(pd_mode_axis_debug_snapshot_t *out);
void pd_mode_zoom_debug_snapshot(pd_mode_axis_debug_snapshot_t *out);
void pd_mode_arrow_debug_snapshot(pd_mode_arrow_debug_snapshot_t *out);
