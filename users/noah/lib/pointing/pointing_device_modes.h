// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Modes
// ────────────────────────────────────────────────────────────────────────────
//
// Full public interface for the pointing-device mode system.
// Implementations are split across pd_mode_registry.c and pd_mode_state.c.
//
// Non-pointing modules that only need mode flag constants or read-only state
// queries should include pd_mode_flags.h instead of this header.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // QMK

#include "noah_keymap.h"   // VOLUME_MODE, BRIGHTNESS_MODE, etc.
#include "pd_mode_flags.h" // PD_MODE_* constants, state queries

#if defined(POINTING_DEVICE_ENABLE)
typedef report_mouse_t (*pd_mode_handler_t)(report_mouse_t);
typedef bool (*pd_mode_key_handler_t)(uint16_t, keyrecord_t *);
#else
typedef void *pd_mode_handler_t; // unused stub — lets the struct compile
typedef void *pd_mode_key_handler_t;
#endif

typedef void (*pd_mode_reset_t)(void);

typedef struct {
    uint8_t               mode_flag;
    uint16_t              keycode;     // keycode that activates this mode (KC_NO = none)
    uint16_t              lock_action; // KC_NO = not lockable; otherwise toggles persistent mode lock
    pd_mode_handler_t     handler;     // NULL = trackball handled externally (e.g. dragscroll)
    pd_mode_key_handler_t key_handler; // optional key-event interception while mode is active
    pd_mode_reset_t       reset;       // called on deactivation (NULL = no-op)
} pd_mode_def_t;

_Static_assert(PD_MODE_KEYCODE_COUNT == PD_MODE_COUNT, "pd-mode keycode count in custom_keycodes enum doesn't match PD_MODE_COUNT — keep the pd-mode keycode block dense and update both together");

extern const pd_mode_def_t pd_modes[PD_MODE_COUNT];

void pd_mode_set(uint8_t mode);
void pd_mode_clear(uint8_t mode);
void pd_mode_set_locked(uint8_t mode);
void pd_mode_clear_locked(uint8_t mode);
void pd_mode_apply_remote_snapshot(uint8_t active_flags, uint8_t locked_flags);

const pd_mode_def_t *pd_mode_lookup(uint8_t mode);
const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action);
bool                 pd_mode_is_lockable(uint8_t mode);
bool                 is_pd_mode_lock_action(uint16_t action);

void pd_mode_activate(uint8_t mode);
void pd_mode_deactivate(uint8_t mode);
void pd_mode_lock(uint8_t mode);
void pd_mode_unlock(uint8_t mode);
bool pd_mode_set_lock_state(uint8_t mode, bool locked);
bool pd_mode_toggle_lock_state(uint8_t mode);
bool pd_mode_unlock_other_locks(uint8_t keep_mode);
bool pd_mode_deactivate_other_unlocked(uint8_t keep_mode);
void pd_mode_update(uint8_t mode, bool active);

bool    pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record);
uint8_t pd_mode_for_keycode(uint16_t keycode);
uint8_t pd_mode_first_active_index(void);
