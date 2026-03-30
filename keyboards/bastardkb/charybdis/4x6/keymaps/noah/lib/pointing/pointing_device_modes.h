// ────────────────────────────────────────────────────────────────────────────
// Pointing Device Modes
// ────────────────────────────────────────────────────────────────────────────
//
// Public interface for the pointing-device mode system.
// Implementation lives in pointing_device_modes.c.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../../keymap_defs.h" // VOLUME_MODE, BRIGHTNESS_MODE, etc.

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

#define PD_MODE_VOLUME (1 << 0)
#define PD_MODE_ARROW (1 << 1)
#define PD_MODE_DRAGSCROLL (1 << 2)
#define PD_MODE_BRIGHTNESS (1 << 3)
#define PD_MODE_ZOOM (1 << 4)
#define PD_MODE_PINCH (1 << 5)
#define PD_MODE_COUNT 6

_Static_assert(PINCH_MODE - VOLUME_MODE + 1 == PD_MODE_COUNT, "pd-mode keycode count in custom_keycodes enum doesn't match PD_MODE_COUNT — update both together");

extern uint8_t             pd_mode_flags;
extern uint8_t             pd_mode_locked_flags;
extern const pd_mode_def_t pd_modes[PD_MODE_COUNT];

void pd_mode_set(uint8_t mode);
void pd_mode_clear(uint8_t mode);
void pd_mode_set_locked(uint8_t mode);
void pd_mode_clear_locked(uint8_t mode);
bool pd_mode_active(uint8_t mode);
bool pd_mode_locked(uint8_t mode);
bool pd_any_mode_locked(void);
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
