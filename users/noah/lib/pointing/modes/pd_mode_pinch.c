// ────────────────────────────────────────────────────────────────────────────
// PD Mode Pinch
// ────────────────────────────────────────────────────────────────────────────
//
// Pinch mode reuses the local dragscroll motion handler, but its modifier and
// lock side effects still belong to the mode itself. Keep those lifecycle
// hooks next to the other mode-owned implementations instead of in the
// registry core.
// ────────────────────────────────────────────────────────────────────────────

#include "../pd_mode_registry_internal.h"
#include "../../state/keyboard_mod_ownership.h"

static bool pinch_command_registered = false;

static void pinch_mode_register_command(pd_mode_mask_t mode) {
    (void)mode;

    if (pinch_command_registered) {
        return;
    }

    // Pinch mode owns a logical GUI hold for trackpad gestures. Keep that on
    // the same real-mod ownership path as the rest of the custom runtime
    // instead of advertising it as a transient weak modifier.
    keyboard_mod_ownership_register(KC_LEFT_GUI);
    pinch_command_registered = true;
}

static void pinch_mode_unregister_command(pd_mode_mask_t mode) {
    (void)mode;

    if (!pinch_command_registered) {
        return;
    }

    keyboard_mod_ownership_unregister(KC_LEFT_GUI);
    pinch_command_registered = false;
}

const pd_mode_lifecycle_hooks_t pd_mode_pinch_lifecycle_hooks = {
    .on_activate   = pinch_mode_register_command,
    .on_deactivate = pinch_mode_unregister_command,
#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
    .on_lock   = pd_mode_scroll_lock_attach_auto_mouse,
    .on_unlock = pd_mode_scroll_lock_detach_auto_mouse,
#endif
};
