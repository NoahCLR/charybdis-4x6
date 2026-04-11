// ────────────────────────────────────────────────────────────────────────────
// PD Modes
// ────────────────────────────────────────────────────────────────────────────
//
// Public cross-module interface for the pd-mode system.
// Implementations are split across pd_mode_registry.c and pd_mode_state.c.
//
// Non-pointing modules that only need mode flag constants or read-only state
// queries should include pd_mode_flags.h instead of this header.
// Low-level state mutators stay private in pd_mode_internal.h so callers
// cannot bypass the mode-state invariants.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "noah_keymap_ids.h" // VOLUME_MODE, BRIGHTNESS_MODE, etc.
#include "pd_mode_flags.h"   // PD_MODE_* constants, state queries

#if defined(POINTING_DEVICE_ENABLE)
typedef report_mouse_t (*pd_mode_handler_t)(report_mouse_t);
typedef bool (*pd_mode_key_handler_t)(uint16_t, keyrecord_t *);
#else
typedef void *pd_mode_handler_t; // unused stub — lets the struct compile
typedef void *pd_mode_key_handler_t;
#endif

typedef void (*pd_mode_reset_t)(void);

typedef struct {
    pd_mode_mask_t        mode_flag;
    uint16_t              keycode;     // keycode that activates this mode (KC_NO = none)
    uint16_t              lock_action; // generated as <MODE_KEYCODE>_LOCK and toggles persistent mode lock
    pd_mode_handler_t     handler;     // NULL = trackball handled externally (e.g. dragscroll)
    pd_mode_key_handler_t key_handler; // optional key-event interception while mode is active
    pd_mode_reset_t       reset;       // called on deactivation (NULL = no-op)
    uint16_t              dpi;         // pointer CPI while this mode is active (0 = use normal pointer DPI)
} pd_mode_def_t;

_Static_assert(PD_MODE_KEYCODE_COUNT == PD_MODE_COUNT, "pd-mode keycode count in custom_keycodes enum doesn't match PD_MODE_COUNT — keep the pd-mode keycode block dense and update both together");

extern const pd_mode_def_t pd_modes[PD_MODE_COUNT];

void pd_mode_apply_remote_snapshot(pd_mode_mask_t active_flags, pd_mode_mask_t locked_flags);

const pd_mode_def_t *pd_mode_lookup(pd_mode_mask_t mode);
const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action);
bool                 is_pd_mode_lock_action(uint16_t action);

bool pd_mode_set_lock_state(pd_mode_mask_t mode, bool locked);
bool pd_mode_toggle_lock_state(pd_mode_mask_t mode);
bool pd_mode_handle_keycode_press(uint16_t keycode);
bool pd_mode_handle_keycode_release(uint16_t keycode);

bool           pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record);
pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode);
uint8_t        pd_mode_first_active_index(void);

// Apply the active mode's DPI, or restore Charybdis's normal pointer DPI if no
// mode with a custom DPI is active. No-op when sniping or dragscroll is active
// since those have their own CPI management.
void pd_mode_apply_active_dpi(void);
