// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode Registry Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Shared private lifecycle/policy surface between pointing-mode definition
// materialization, mode-owned hook objects, and the lifecycle transition
// implementation. Keep this out of the public pointing-mode headers so external
// callers still go through the normal mode API.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "../defs/pointing_modes.h"

struct pointing_mode_lifecycle_hooks {
    void (*on_activate)(pointing_mode_mask_t mode);
    void (*on_deactivate)(pointing_mode_mask_t mode);
    void (*on_lock)(pointing_mode_mask_t mode);
    void (*on_unlock)(pointing_mode_mask_t mode);
};

#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
void pointing_mode_scroll_lock_attach_auto_mouse(pointing_mode_mask_t mode);
void pointing_mode_scroll_lock_detach_auto_mouse(pointing_mode_mask_t mode);
#endif

void pointing_mode_registry_run_activate_hooks(pointing_mode_mask_t mode);
void pointing_mode_registry_run_deactivate_hooks(pointing_mode_mask_t mode);
void pointing_mode_registry_run_lock_hooks(pointing_mode_mask_t mode);
void pointing_mode_registry_run_unlock_hooks(pointing_mode_mask_t mode);
