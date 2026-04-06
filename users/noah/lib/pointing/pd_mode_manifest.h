// ────────────────────────────────────────────────────────────────────────────
// PD Mode Manifest
// ────────────────────────────────────────────────────────────────────────────
//
// Single source of truth for pd-mode identity and registry metadata.
// Keep mode keycodes, lock keycodes, flags, handlers, and registry rows in
// sync by generating them from this list.
//
// To add a new shared pd mode, add exactly one row to NOAH_PD_MODE_LIST.
// Do not hand-edit the generated keycode enum, flag enum, or registry table.
//
// Row format:
//   PDM(NAME, MODE_KEYCODE, POINTER_HANDLER, KEY_HANDLER, RESET_FN, DPI_OVERRIDE)
//
// Field meanings:
//   NAME:
//     Uppercase logical mode id. Generates symbols such as PD_MODE_NAME and
//     PD_MODE_INDEX_NAME. Example: VOLUME -> PD_MODE_VOLUME.
//   MODE_KEYCODE:
//     The custom keycode symbol for the mode's momentary key. This symbol is
//     declared by the generated custom_keycodes enum from this manifest.
//     Example: VOLUME_MODE.
//     The matching persistent lock keycode is generated automatically as
//     MODE_KEYCODE + _LOCK.
//   POINTER_HANDLER:
//     Optional pointer/mouse handler function while the mode is active.
//     Use NULL if the mode has no pointer-motion behavior.
//   KEY_HANDLER:
//     Optional key-event interceptor while the mode is active.
//     Use NULL if the mode does not need to rewrite key events.
//   RESET_FN:
//     Optional cleanup hook called when the mode is deactivated or unlocked.
//     Use NULL if the mode has no teardown work.
//   DPI_OVERRIDE:
//     Pointer CPI to apply while the mode is active. Use a PD_MODE_*_DPI macro
//     for normal modes, or 0 to keep the current/default pointer DPI.
//
// Example:
//   PDM(MY_NEW_MODE, MY_NEW_MODE_KEY,
//     handle_my_new_mode, NULL, reset_my_new_mode,
//     PD_MODE_MY_NEW_MODE_DPI)
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#define NOAH_PD_MODE_LIST(PDM)                                                                                                             \
    PDM(DRAGSCROLL, DRAGSCROLL, NULL, NULL, NULL, 0)                                                                                      \
    PDM(VOLUME, VOLUME_MODE, handle_volume_mode, NULL, reset_volume_mode, PD_MODE_VOLUME_DPI)                                            \
    PDM(BRIGHTNESS, BRIGHTNESS_MODE, handle_brightness_mode, NULL, reset_brightness_mode, PD_MODE_BRIGHTNESS_DPI)                       \
    PDM(ZOOM, ZOOM_MODE, handle_zoom_mode, NULL, reset_zoom_mode, PD_MODE_ZOOM_DPI)                                                     \
    PDM(ARROW, ARROW_MODE, handle_arrow_mode, handle_arrow_mode_key, reset_arrow_mode, PD_MODE_ARROW_DPI)                               \
    PDM(PINCH, PINCH_MODE, NULL, NULL, NULL, 0)
