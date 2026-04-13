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
//   PDM(NAME, MODE_KEYCODE, POINTER_HANDLER, KEY_HANDLER, RESET_FN, DPI_OVERRIDE, TRAITS, LIFECYCLE)
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
//   TRAITS:
//     Bitmask of PD_MODE_TRAIT_* flags that describe cross-cutting policy for
//     pointer-layer anchoring, dragscroll-handler DPI ownership, lock behavior,
//     and other shared registry/pointer policy. Prefer adding a new trait when
//     multiple modes share the same policy.
//   LIFECYCLE:
//     Optional lifecycle hook object selected by this definition row. Prefer
//     mode-owned hook objects for mode-specific activate / deactivate / lock /
//     unlock side effects, and keep registry-owned objects for genuinely
//     shared policy. Use NULL when the mode needs no custom lifecycle work
//     beyond shared trait policy.
//
// Example:
//   PDM(MY_NEW_MODE, MY_NEW_MODE_KEY,
//     handle_my_new_mode, NULL, reset_my_new_mode,
//     PD_MODE_MY_NEW_MODE_DPI, PD_MODE_TRAIT_NONE, NULL)
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

typedef uint16_t pd_mode_traits_t;

enum {
    PD_MODE_TRAIT_NONE                        = 0,
    PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED    = (pd_mode_traits_t)1u << 0,
    PD_MODE_TRAIT_PREFER_TYPING_LAYER         = (pd_mode_traits_t)1u << 1,
    PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND   = (pd_mode_traits_t)1u << 2,
    PD_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE = (pd_mode_traits_t)1u << 3,
};

#define NOAH_PD_MODE_LIST(PDM)                                                                                                                                                                                                                           \
    PDM(DRAGSCROLL, DRAGSCROLL, handle_dragscroll_mode, NULL, reset_dragscroll_mode, 0, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED | PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND | PD_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE, PD_MODE_LIFECYCLE_AUTO_MOUSE_LOCK) \
    PDM(VOLUME, VOLUME_MODE, handle_volume_mode, NULL, reset_volume_mode, PD_MODE_VOLUME_DPI, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED, NULL)                                                                                                              \
    PDM(BRIGHTNESS, BRIGHTNESS_MODE, handle_brightness_mode, NULL, reset_brightness_mode, PD_MODE_BRIGHTNESS_DPI, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED, NULL)                                                                                          \
    PDM(ZOOM, ZOOM_MODE, handle_zoom_mode, NULL, reset_zoom_mode, PD_MODE_ZOOM_DPI, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED, NULL)                                                                                                                        \
    PDM(ARROW, ARROW_MODE, handle_arrow_mode, handle_arrow_mode_key, reset_arrow_mode, PD_MODE_ARROW_DPI, PD_MODE_TRAIT_PREFER_TYPING_LAYER, NULL)                                                                                                       \
    PDM(PINCH, PINCH_MODE, handle_dragscroll_mode, NULL, reset_dragscroll_mode, 0, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED | PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND | PD_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE, PD_MODE_LIFECYCLE_PINCH)
