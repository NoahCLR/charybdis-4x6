// ────────────────────────────────────────────────────────────────────────────
// Pointing Modes
// ────────────────────────────────────────────────────────────────────────────
//
// Public cross-module interface for the pointing-mode system.
// Runtime ownership is split across registry/state/lifecycle core files, with
// mode-owned behavior under pointing/modes/.
//
// Non-pointing modules that only need mode flag constants or read-only state
// queries should include pointing_mode_flags.h instead of this header.
// Low-level state mutators stay private in pointing_mode_internal.h so callers
// cannot bypass the mode-state invariants.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "noah_keymap_ids.h"     // PD_SLOT_1, PD_SLOT_2, etc.
#include "pointing_mode_flags.h" // POINTING_MODE_* constants, state queries

typedef struct pointing_mode_lifecycle_hooks pointing_mode_lifecycle_hooks_t;

// A slot's deployed action identities. What the slot does (motion, keys, DPI,
// policy) is its record in the active profile, read through
// active_profile_pd_runtime.h.
typedef struct {
    pointing_mode_mask_t mode_flag;
    uint16_t             keycode;     // keycode that activates this mode (KC_NO = none)
    uint16_t             lock_action; // generated as <MODE_KEYCODE>_LOCK and toggles persistent mode lock
} pointing_mode_def_t;

_Static_assert(POINTING_MODE_KEYCODE_COUNT == POINTING_MODE_COUNT, "pointing-mode keycode count in custom_keycodes enum doesn't match POINTING_MODE_COUNT — keep the pointing-mode keycode block dense and update both together");

typedef enum {
    POINTING_MODE_COMMAND_NONE = 0,
    POINTING_MODE_COMMAND_ACTIVATE,
    POINTING_MODE_COMMAND_DEACTIVATE,
    POINTING_MODE_COMMAND_LOCK,
    POINTING_MODE_COMMAND_UNLOCK,
    POINTING_MODE_COMMAND_KEY_PRESS,
    POINTING_MODE_COMMAND_KEY_RELEASE,
    POINTING_MODE_COMMAND_REMOTE_SNAPSHOT,
} pointing_mode_command_kind_t;

typedef struct {
    pointing_mode_command_kind_t kind;
    uint16_t                     keycode;
    pointing_mode_mask_t         mode;
    pointing_mode_id_t           active_mode_id;
    pointing_mode_id_t           locked_mode_id;
    split_side_mask_t            owner_sides;
    keypos_t                     owner_key_pos;
    const uint8_t               *owner_bitmap;
} pointing_mode_command_t;

typedef struct {
    pointing_mode_snapshot_t before;
    pointing_mode_snapshot_t after;
    bool                     handled;
    bool                     local_state_changed;
    bool                     display_state_changed;
} pointing_mode_apply_result_t;

extern const pointing_mode_def_t pointing_modes[POINTING_MODE_COUNT];

pointing_mode_apply_result_t pointing_mode_apply_command(pointing_mode_command_t command);
void                         pointing_mode_apply_remote_mode_ids(pointing_mode_id_t active_mode_id, pointing_mode_id_t locked_mode_id, split_side_mask_t owner_sides);
void                         pointing_mode_apply_remote_mode_ids_with_owner_bitmap(pointing_mode_id_t active_mode_id, pointing_mode_id_t locked_mode_id, split_side_mask_t owner_sides, const uint8_t *owner_bitmap);
void                         pointing_mode_apply_remote_snapshot(pointing_mode_mask_t active_flags, pointing_mode_mask_t locked_flags);

const pointing_mode_def_t *pointing_mode_lookup(pointing_mode_mask_t mode);
const pointing_mode_def_t *pointing_mode_lock_action_lookup(uint16_t action);
bool                       is_pointing_mode_lock_action(uint16_t action);
bool                       pointing_mode_has_trait(pointing_mode_mask_t mode, pointing_mode_traits_t trait);
bool                       pd_any_active_mode_has_trait(pointing_mode_traits_t trait);

bool pointing_mode_set_lock_state(pointing_mode_mask_t mode, bool locked);
bool pointing_mode_toggle_lock_state(pointing_mode_mask_t mode);
bool pointing_mode_set_lock_state_at(pointing_mode_mask_t mode, bool locked, keypos_t key_pos);
bool pointing_mode_toggle_lock_state_at(pointing_mode_mask_t mode, keypos_t key_pos);
bool pointing_mode_local_owner_key_pos_snapshot(keypos_t *out);
bool pointing_mode_handle_keycode_press(uint16_t keycode);
bool pointing_mode_handle_keycode_release(uint16_t keycode);
bool pointing_mode_handle_keycode_press_at(uint16_t keycode, keypos_t key_pos);
bool pointing_mode_handle_keycode_release_at(uint16_t keycode, keypos_t key_pos);
bool pointing_mode_local_owner_bitmap_snapshot(uint8_t *out_bitmap);
bool pointing_mode_display_owner_bitmap_snapshot(uint8_t *out_bitmap);

bool                 pointing_mode_handle_key_event(uint16_t keycode, keyrecord_t *record);
pointing_mode_mask_t pointing_mode_for_keycode(uint16_t keycode);

// Queue or service the active mode's DPI policy on the normal scan path
// instead of writing pointing hardware from synchronous key/layer hooks.
void pointing_mode_request_active_dpi_sync(void);
void pointing_mode_service_active_dpi_sync(void);
void pointing_mode_set_auto_sniping_layer_active(bool active);
bool pointing_mode_auto_sniping_layer_active(void);

// Apply the active mode's DPI, or restore Charybdis's normal pointer DPI if no
// mode with a custom DPI is active. A scrolling slot uses its own DPI (or the
// normal one) even while sniping; otherwise auto/manual sniping uses
// Charybdis's configured sniping CPI before the slot's DPI.
void pointing_mode_apply_active_dpi(void);

// The policy traits the mode's effective slot implies; none for a disabled or
// unknown mode.
pointing_mode_traits_t pointing_mode_effective_traits(pointing_mode_mask_t mode);
