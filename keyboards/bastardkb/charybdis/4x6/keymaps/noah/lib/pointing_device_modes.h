// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Modes
// ────────────────────────────────────────────────────────────────────────────
//
// This module lets the trackball do different things depending on which
// "mode" is active.  Mode keys are dual-purpose: tap sends the base-layer
// key at that position, hold activates the trackball mode.
//
// Available modes:
//
//   VOLUME mode  — Trackball Y-axis controls system volume.
//                  Roll up = volume up, roll down = volume down.
//
//   ARROW mode   — Trackball motion sends arrow key presses.
//                  Dominant axis wins: horizontal motion sends Left/Right,
//                  vertical motion sends Up/Down.  Respects Shift for
//                  text selection.
//
//   BRIGHTNESS mode — Trackball Y-axis controls screen brightness.
//                    Roll up = brighter, roll down = dimmer.
//
//   ZOOM mode    — Trackball Y-axis controls zoom level.
//                  Roll up = zoom in (GUI+Plus), roll down = zoom out (GUI+Minus).
//
//   DRAGSCROLL   — Not handled here; this flag just tracks whether the
//                  Charybdis firmware's native drag-scroll is active,
//                  so RGB can reflect the state.
//
// How to add a new mode: see INTERNALS.md → "Add a new trackball mode".
// Short version: define a flag here, write a handler in
// pointing_device_mode_handlers.h, add it to pd_modes[] here, then add a
// keycode and color in the other config files.
// Everything else (key handling, mouse record, dispatch, RGB) is automatic.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "key_config.h" // VOLUME_MODE, BRIGHTNESS_MODE, etc.

// ─── Mode handler type ──────────────────────────────────────────────────
// NULL means the mode is handled externally (e.g. dragscroll by charybdis firmware).
#if defined(POINTING_DEVICE_ENABLE)
typedef report_mouse_t (*pd_mode_handler_t)(report_mouse_t);
#else
typedef void *pd_mode_handler_t; // unused stub — lets the struct compile
#endif

// ─── Mode definition struct ─────────────────────────────────────────────
// Each mode has a flag, an optional custom keycode, and a handler function.
// Array order = priority order — first active mode wins for both handler
// dispatch and RGB overlay.  KC_NO means the mode has no dedicated keycode
// (e.g. dragscroll is activated via Charybdis firmware keycodes).

typedef struct {
    uint8_t           mode_flag;
    uint16_t          keycode; // KC_NO for firmware-handled modes
    pd_mode_handler_t handler; // NULL for firmware-handled modes
} pd_mode_def_t;

// ─── Global mode state ─────────────────────────────────────────────────────
// Single translation unit (keymap.c includes this header) — static is safe.
// If this were included from multiple .c files, each would get its own copy.
static uint8_t pd_mode_flags = 0;

static inline void pd_mode_set(uint8_t mode) {
    pd_mode_flags |= mode;
}
static inline void pd_mode_clear(uint8_t mode) {
    pd_mode_flags &= ~mode;
}
static inline bool pd_mode_active(uint8_t mode) {
    return (pd_mode_flags & mode) != 0;
}

// Convenience: set or clear a mode based on a boolean.
// Typical use: pd_mode_update(PD_MODE_VOLUME, record->event.pressed)
//   → activates on key-down, deactivates on key-up.
static inline void pd_mode_update(uint8_t mode, bool active) {
    if (active)
        pd_mode_set(mode);
    else
        pd_mode_clear(mode);
}

// ─── Handler implementations ─────────────────────────────────────────────
// Included here (after the types and flags they depend on, before pd_modes[]
// which references the function pointers).  Handler logic lives in its own
// file so tuning algorithms doesn't require touching mode plumbing.
#include "pointing_device_mode_handlers.h"

// ─── Mode flags (bitfield) ────────────────────────────────────────────────
// Each mode is a single bit.  Multiple modes could theoretically be active
// at once; pd_modes[] defines which one wins (checked first = highest).

#define PD_MODE_VOLUME (1 << 0)     // Trackball Y → volume up/down
#define PD_MODE_ARROW (1 << 1)      // Trackball → arrow keys
#define PD_MODE_DRAGSCROLL (1 << 2) // Mirrors charybdis_get_pointer_dragscroll_enabled()
#define PD_MODE_BRIGHTNESS (1 << 3) // Trackball Y → screen brightness up/down
#define PD_MODE_ZOOM (1 << 4)       // Trackball Y → GUI+Plus / GUI+Minus
// #define PD_MODE_xxx       (1 << 5)  // next free slot
// ... up to (1 << 7) for 8 modes in a uint8_t

// ─── Mode config table ──────────────────────────────────────────────────────
// Defined after the handler functions so the function pointers resolve.
// NULL handler means the mode is handled externally (dragscroll by firmware).

// mode_flag            keycode          handler
static const pd_mode_def_t pd_modes[] = {
    {PD_MODE_DRAGSCROLL, KC_NO, NULL},                             //
    {PD_MODE_VOLUME, VOLUME_MODE, handle_volume_mode},             //
    {PD_MODE_BRIGHTNESS, BRIGHTNESS_MODE, handle_brightness_mode}, //
    {PD_MODE_ZOOM, ZOOM_MODE, handle_zoom_mode},                   //
    {PD_MODE_ARROW, ARROW_MODE, handle_arrow_mode},                //
};

#define PD_MODE_COUNT (sizeof(pd_modes) / sizeof(pd_modes[0]))

// Look up which mode flag a keycode activates.  Returns 0 if not found.
static inline uint8_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode) return pd_modes[i].mode_flag;
    }
    return 0;
}
