// ────────────────────────────────────────────────────────────────────────────
// PD Modes
// ────────────────────────────────────────────────────────────────────────────
//
// Public cross-module interface for the pd-mode system.
// Runtime ownership is split across registry/state/lifecycle core files, with
// mode-owned behavior under pointing/modes/.
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
typedef struct pd_mode_lifecycle_hooks pd_mode_lifecycle_hooks_t;

typedef struct {
    pd_mode_mask_t                   mode_flag;
    uint16_t                         keycode;     // keycode that activates this mode (KC_NO = none)
    uint16_t                         lock_action; // generated as <MODE_KEYCODE>_LOCK and toggles persistent mode lock
    pd_mode_handler_t                handler;     // NULL = mode has no pointer-motion transform
    pd_mode_key_handler_t            key_handler; // optional key-event interception while mode is active
    pd_mode_reset_t                  reset;       // called on deactivation (NULL = no-op)
    uint16_t                         dpi;         // pointer CPI while this mode is active (0 = use normal pointer DPI)
    pd_mode_traits_t                 traits;      // manifest-defined policy flags consumed by registry/pointer policy
    const pd_mode_lifecycle_hooks_t *lifecycle;   // optional activate/deactivate/lock/unlock side effects owned by this definition row
} pd_mode_def_t;

_Static_assert(PD_MODE_KEYCODE_COUNT == PD_MODE_COUNT, "pd-mode keycode count in custom_keycodes enum doesn't match PD_MODE_COUNT — keep the pd-mode keycode block dense and update both together");

typedef enum {
    PD_MODE_COMMAND_NONE = 0,
    PD_MODE_COMMAND_ACTIVATE,
    PD_MODE_COMMAND_DEACTIVATE,
    PD_MODE_COMMAND_LOCK,
    PD_MODE_COMMAND_UNLOCK,
    PD_MODE_COMMAND_KEY_PRESS,
    PD_MODE_COMMAND_KEY_RELEASE,
    PD_MODE_COMMAND_REMOTE_SNAPSHOT,
} pd_mode_command_kind_t;

typedef struct {
    pd_mode_command_kind_t kind;
    uint16_t               keycode;
    pd_mode_mask_t         mode;
    pd_mode_mask_t         active_flags;
    pd_mode_mask_t         locked_flags;
} pd_mode_command_t;

typedef struct {
    pd_mode_snapshot_t before;
    pd_mode_snapshot_t after;
    bool               handled;
    bool               local_state_changed;
    bool               display_state_changed;
    bool               split_sync_required;
} pd_mode_apply_result_t;

extern const pd_mode_def_t pd_modes[PD_MODE_COUNT];

pd_mode_apply_result_t pd_mode_apply_command(pd_mode_command_t command);
void                   pd_mode_apply_remote_snapshot(pd_mode_mask_t active_flags, pd_mode_mask_t locked_flags);

const pd_mode_def_t *pd_mode_lookup(pd_mode_mask_t mode);
const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action);
bool                 is_pd_mode_lock_action(uint16_t action);
bool                 pd_mode_has_trait(pd_mode_mask_t mode, pd_mode_traits_t trait);
bool                 pd_any_active_mode_has_trait(pd_mode_traits_t trait);

bool pd_mode_set_lock_state(pd_mode_mask_t mode, bool locked);
bool pd_mode_toggle_lock_state(pd_mode_mask_t mode);
bool pd_mode_handle_keycode_press(uint16_t keycode);
bool pd_mode_handle_keycode_release(uint16_t keycode);

bool           pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record);
pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode);

// Apply the active mode's DPI, or restore Charybdis's normal pointer DPI if no
// mode with a custom DPI is active. Dragscroll-like modes use the shared local
// dragscroll DPI; sniping still re-applies Charybdis-owned CPI.
void pd_mode_apply_active_dpi(void);
