// ────────────────────────────────────────────────────────────────────────────
// PD Mode Manifest
// ────────────────────────────────────────────────────────────────────────────
//
// Single source of truth for pd-mode identity and registry metadata.
// Keep mode keycodes, lock keycodes, flags, handlers, and registry rows in
// sync by generating them from this list.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#define NOAH_PD_MODE_LIST(M)                                                                                                               \
    M(DRAGSCROLL, DRAGSCROLL, DRAGSCROLL_LOCK, NULL, NULL, NULL, 0)                                                                       \
    M(VOLUME, VOLUME_MODE, VOLUME_MODE_LOCK, handle_volume_mode, NULL, reset_volume_mode, PD_MODE_VOLUME_DPI)                            \
    M(BRIGHTNESS, BRIGHTNESS_MODE, BRIGHTNESS_MODE_LOCK, handle_brightness_mode, NULL, reset_brightness_mode, PD_MODE_BRIGHTNESS_DPI)   \
    M(ZOOM, ZOOM_MODE, ZOOM_MODE_LOCK, handle_zoom_mode, NULL, reset_zoom_mode, PD_MODE_ZOOM_DPI)                                       \
    M(ARROW, ARROW_MODE, ARROW_MODE_LOCK, handle_arrow_mode, handle_arrow_mode_key, reset_arrow_mode, PD_MODE_ARROW_DPI)                \
    M(PINCH, PINCH_MODE, PINCH_MODE_LOCK, NULL, NULL, NULL, 0)
