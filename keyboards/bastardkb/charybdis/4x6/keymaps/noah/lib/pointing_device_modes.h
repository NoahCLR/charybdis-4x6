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
//   DRAGSCROLL   — Momentary drag-scroll while held.  The actual trackball
//                  behavior is handled by the Charybdis firmware; pd_mode_update
//                  keeps charybdis_set_pointer_dragscroll_enabled in sync.
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
typedef bool (*pd_mode_key_handler_t)(uint16_t, keyrecord_t *);
#else
typedef void *pd_mode_handler_t; // unused stub — lets the struct compile
typedef void *pd_mode_key_handler_t;
#endif

// Reset callback: called when a mode deactivates to clear accumulated state.
typedef void (*pd_mode_reset_t)(void);

// ─── Mode definition struct ─────────────────────────────────────────────
// Each mode has a flag, an optional custom keycode, and optional handlers for
// trackball motion and key events while the mode is active.
// Array order = priority order — first active mode wins for motion dispatch,
// key interception, and RGB overlay.

typedef struct {
    uint8_t           mode_flag;
    uint16_t          keycode;   // keycode that activates this mode (KC_NO = none)
    uint16_t          lock_action; // KC_NO = not lockable; otherwise toggles persistent mode lock
    pd_mode_handler_t handler;   // NULL = trackball handled externally (e.g. dragscroll)
    pd_mode_key_handler_t key_handler; // optional key-event interception while mode is active
    pd_mode_reset_t   reset;     // called on deactivation (NULL = no-op)
} pd_mode_def_t;

// ─── Global mode state ─────────────────────────────────────────────────────
// Single translation unit (keymap.c includes this header) — static is safe.
// If this were included from multiple .c files, each would get its own copy.
static uint8_t pd_mode_flags = 0;
static uint8_t pd_mode_locked_flags = 0;

static inline void pd_mode_set(uint8_t mode) {
    pd_mode_flags |= mode;
}
static inline void pd_mode_clear(uint8_t mode) {
    pd_mode_flags &= ~mode;
}
static inline void pd_mode_set_locked(uint8_t mode) {
    pd_mode_locked_flags |= mode;
}
static inline void pd_mode_clear_locked(uint8_t mode) {
    pd_mode_locked_flags &= ~mode;
}
static inline bool pd_mode_active(uint8_t mode) {
    return (pd_mode_flags & mode) != 0;
}
static inline bool pd_mode_locked(uint8_t mode) {
    return (pd_mode_locked_flags & mode) != 0;
}
static inline bool pd_any_mode_locked(void) {
    return pd_mode_locked_flags != 0;
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

// mode_flag            keycode          lock_action                     handler                  key_handler             reset
static const pd_mode_def_t pd_modes[] = {
    {PD_MODE_DRAGSCROLL, DRAGSCROLL, LOCK_PD_MODE(DRAGSCROLL), NULL, NULL, NULL},                                     //
    {PD_MODE_VOLUME, VOLUME_MODE, LOCK_PD_MODE(VOLUME_MODE), handle_volume_mode, NULL, reset_volume_mode},            //
    {PD_MODE_BRIGHTNESS, BRIGHTNESS_MODE, LOCK_PD_MODE(BRIGHTNESS_MODE), handle_brightness_mode, NULL, reset_brightness_mode}, //
    {PD_MODE_ZOOM, ZOOM_MODE, LOCK_PD_MODE(ZOOM_MODE), handle_zoom_mode, NULL, reset_zoom_mode},                      //
    {PD_MODE_ARROW, ARROW_MODE, LOCK_PD_MODE(ARROW_MODE), handle_arrow_mode, handle_arrow_mode_key, reset_arrow_mode}, //
};

#define PD_MODE_COUNT (sizeof(pd_modes) / sizeof(pd_modes[0]))

static inline const pd_mode_def_t *pd_mode_lookup(uint8_t mode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].mode_flag == mode) return &pd_modes[i];
    }
    return NULL;
}

static inline const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].lock_action != KC_NO && pd_modes[i].lock_action == action) return &pd_modes[i];
    }
    return NULL;
}

static inline bool is_pd_mode_lock_action(uint16_t action) {
    return pd_mode_lock_action_lookup(action) != NULL;
}

static inline bool pd_mode_is_lockable(uint8_t mode) {
    const pd_mode_def_t *def = pd_mode_lookup(mode);
    return def && def->lock_action != KC_NO;
}

static inline void pd_mode_activate(uint8_t mode) {
    pd_mode_set(mode);
    if (mode == PD_MODE_DRAGSCROLL) {
        charybdis_set_pointer_dragscroll_enabled(true);
    }
}

static inline void pd_mode_deactivate(uint8_t mode) {
    pd_mode_clear(mode);
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].mode_flag == mode && pd_modes[i].reset) {
            pd_modes[i].reset();
            break;
        }
    }
    if (mode == PD_MODE_DRAGSCROLL) {
        charybdis_set_pointer_dragscroll_enabled(false);
    }
}

static inline void pd_mode_lock(uint8_t mode) {
    pd_mode_set_locked(mode);
    pd_mode_activate(mode);
}

static inline void pd_mode_unlock(uint8_t mode) {
    pd_mode_clear_locked(mode);
    pd_mode_deactivate(mode);
}

// Lock state adds a small amount of policy on top of the raw mode primitives:
// only one pd mode may stay locked at a time, and dragscroll mirrors the
// auto-mouse toggle so the pointer layer stays anchored while locked.
static inline bool pd_mode_set_lock_state(uint8_t mode, bool locked) {
    if (locked) {
        bool changed = false;

        for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
            uint8_t other_mode = pd_modes[i].mode_flag;
            if (other_mode != mode && pd_mode_locked(other_mode)) {
                pd_mode_unlock(other_mode);
                changed = true;
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
                if (other_mode == PD_MODE_DRAGSCROLL && get_auto_mouse_toggle()) {
                    auto_mouse_toggle();
                }
#endif
            }
        }

        if (!pd_mode_locked(mode)) {
            pd_mode_lock(mode);
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
            if (mode == PD_MODE_DRAGSCROLL && !get_auto_mouse_toggle()) {
                auto_mouse_toggle();
            }
#endif
            changed = true;
        }

        return changed;
    }

    if (!pd_mode_locked(mode)) return false;

    pd_mode_unlock(mode);

#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (mode == PD_MODE_DRAGSCROLL && get_auto_mouse_toggle()) {
        auto_mouse_toggle();
    }
#endif

    return true;
}

static inline bool pd_mode_toggle_lock_state(uint8_t mode) {
    return pd_mode_set_lock_state(mode, !pd_mode_locked(mode));
}

static inline bool pd_mode_unlock_other_locks(uint8_t keep_mode) {
    bool changed = false;

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        uint8_t mode = pd_modes[i].mode_flag;
        if (mode != keep_mode) {
            changed |= pd_mode_set_lock_state(mode, false);
        }
    }

    return changed;
}

// Convenience: set or clear a mode based on a boolean, resetting the
// handler's accumulators on deactivation so stale state doesn't carry
// into the next activation.
//
// For PD_MODE_DRAGSCROLL the actual trackball behavior lives in the
// Charybdis firmware (charybdis_set_pointer_dragscroll_enabled).
// We keep it in sync here so callers don't need to manage two states.
static inline void pd_mode_update(uint8_t mode, bool active) {
    if (active) {
        pd_mode_activate(mode);
    } else if (!pd_mode_locked(mode)) {
        pd_mode_deactivate(mode);
    }
}

// Give active modes first shot at key events using the same priority order as
// trackball-motion dispatch. A handler returns true when it consumed the event.
static inline bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_modes[i].mode_flag) && pd_modes[i].key_handler &&
            pd_modes[i].key_handler(keycode, record)) {
            return true;
        }
    }
    return false;
}

// Look up which mode flag a keycode activates.  Returns 0 if not found.
static inline uint8_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode) return pd_modes[i].mode_flag;
    }
    return 0;
}
