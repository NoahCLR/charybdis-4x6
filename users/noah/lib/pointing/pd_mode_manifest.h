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
//   M(NAME, MODE_KEYCODE, LOCK_KEYCODE, POINTER_HANDLER, KEY_HANDLER, RESET_FN, DPI_OVERRIDE)
//
// Field meanings:
//   NAME:
//     Uppercase logical mode id. Generates symbols such as PD_MODE_NAME and
//     PD_MODE_INDEX_NAME. Example: VOLUME -> PD_MODE_VOLUME.
//   MODE_KEYCODE:
//     The custom keycode symbol for the mode's momentary key. This symbol is
//     declared by the generated custom_keycodes enum from this manifest.
//     Example: VOLUME_MODE.
//   LOCK_KEYCODE:
//     The custom keycode symbol for the mode's persistent lock toggle.
//     Usually MODE_KEYCODE + _LOCK. Set to KC_NO if this mode must not have a
//     lock action.
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
//   M(MY_NEW_MODE, MY_NEW_MODE_KEY, MY_NEW_MODE_KEY_LOCK,
//     handle_my_new_mode, NULL, reset_my_new_mode,
//     PD_MODE_MY_NEW_MODE_DPI)
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#define NOAH_PD_MODE_LIST(M)                                                                                                               \
    M(DRAGSCROLL, DRAGSCROLL, DRAGSCROLL_LOCK, NULL, NULL, NULL, 0)                                                                       \
    M(VOLUME, VOLUME_MODE, VOLUME_MODE_LOCK, handle_volume_mode, NULL, reset_volume_mode, PD_MODE_VOLUME_DPI)                            \
    M(BRIGHTNESS, BRIGHTNESS_MODE, BRIGHTNESS_MODE_LOCK, handle_brightness_mode, NULL, reset_brightness_mode, PD_MODE_BRIGHTNESS_DPI)   \
    M(ZOOM, ZOOM_MODE, ZOOM_MODE_LOCK, handle_zoom_mode, NULL, reset_zoom_mode, PD_MODE_ZOOM_DPI)                                       \
    M(ARROW, ARROW_MODE, ARROW_MODE_LOCK, handle_arrow_mode, handle_arrow_mode_key, reset_arrow_mode, PD_MODE_ARROW_DPI)                \
    M(PINCH, PINCH_MODE, PINCH_MODE_LOCK, NULL, NULL, NULL, 0)
