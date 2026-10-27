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

#include "noah_keymap_ids.h"     // PD_MODE_1, PD_MODE_2, etc.
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
    // The layers changed to layer_state: the layer links decide what runs
    // when no key runs a mode (D-F62). The layer-state hook sends it.
    POINTING_MODE_COMMAND_LAYER_STATE,
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
    layer_state_t                layer_state;
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
// The layer-state hook's command.
void                         pointing_mode_apply_layer_state(layer_state_t state);
// After a new profile generation or working copy is published: the links it
// holds are evaluated against the layers that are on (D-F62).
void                         pointing_mode_reevaluate_layer_links(void);
// Before the pointing slots are republished: a mode its layer started ends,
// so no engine holds a slot that changes (D-F62).
void                         pointing_mode_end_layer_started_mode(void);
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

// Something other than the policy above wrote the sensor's CPI: the sensor's
// own init, or QMK's Charybdis code (its keys, its DPI steps, its EEPROM
// reset). The next apply writes the policy's CPI even if it is the one it
// wrote last. Every such writer calls this, after its write or before it with
// no apply between.
void pointing_mode_forget_applied_dpi(void);

// A brownout or glitch can reset the sensor to its own default CPI without
// resetting the MCU, and nothing sees it: the policy writes only a CPI that
// changed. So it writes the CPI it intends again this long after anything
// last wrote the sensor's, even unchanged: one write (two register writes and
// their busy-waits, about 360 us) every five seconds.
#define POINTING_MODE_CPI_REFRESH_MS 5000u

// Once a main-loop pass on the master half, after the scan, with the pass's
// one clock read: writes the policy's CPI again once a refresh period has
// passed since the last write. On a master without the sensor the write only
// hands QMK the CPI it already sends; the sensor's half compares the sensor's
// CPI with it every pass.
void pointing_mode_refresh_applied_dpi(uint32_t now);

// The policy traits the mode's effective slot implies; none for a disabled or
// unknown mode.
pointing_mode_traits_t pointing_mode_effective_traits(pointing_mode_mask_t mode);

// Links, from the effective slots (D-F62). A default layer or the pointer
// layer in a link is inert: it starts nothing and is never turned on.
// The mode whose highest linked layer that is on in `state` ranks highest,
// unless what its link says ends it is on too (a higher layer, or any other
// layer, neither counting the pointer layer or a default layer). Zero when
// none.
pointing_mode_mask_t pointing_mode_layer_started_mode(layer_state_t state);
// The live layers `mode`'s key turns on while it runs the mode, bit n layer n;
// any layers, including ones that start another mode.
uint16_t             pointing_mode_key_turns_on(pointing_mode_mask_t mode);
