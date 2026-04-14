// ────────────────────────────────────────────────────────────────────────────
// PD Mode Registry Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Shared private lifecycle/policy surface between pd-mode definition
// materialization, mode-owned hook objects, and the lifecycle transition
// implementation. Keep this out of the public pd-mode headers so external
// callers still go through the normal mode API.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../defs/pd_modes.h"

struct pd_mode_lifecycle_hooks {
    void (*on_activate)(pd_mode_mask_t mode);
    void (*on_deactivate)(pd_mode_mask_t mode);
    void (*on_lock)(pd_mode_mask_t mode);
    void (*on_unlock)(pd_mode_mask_t mode);
    uint8_t (*keyboard_event_masked_real_mods)(pd_mode_mask_t mode);
    uint8_t (*buffered_tap_masked_real_mods)(pd_mode_mask_t mode);
};

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
void pd_mode_scroll_lock_attach_auto_mouse(pd_mode_mask_t mode);
void pd_mode_scroll_lock_detach_auto_mouse(pd_mode_mask_t mode);
#endif

extern const pd_mode_lifecycle_hooks_t pd_mode_pinch_lifecycle_hooks;

void pd_mode_registry_run_activate_hooks(pd_mode_mask_t mode);
void pd_mode_registry_run_deactivate_hooks(pd_mode_mask_t mode);
void pd_mode_registry_run_lock_hooks(pd_mode_mask_t mode);
void pd_mode_registry_run_unlock_hooks(pd_mode_mask_t mode);
