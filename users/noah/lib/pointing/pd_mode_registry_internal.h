// ────────────────────────────────────────────────────────────────────────────
// PD Mode Registry Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Shared private hook-runner surface between pd-mode definition materialization
// and the lifecycle transition implementation. Keep this out of the public
// pd-mode headers so external callers still go through the normal mode API.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "pd_modes.h"

void pd_mode_registry_run_activate_hooks(pd_mode_mask_t mode);
void pd_mode_registry_run_deactivate_hooks(pd_mode_mask_t mode);
void pd_mode_registry_run_lock_hooks(pd_mode_mask_t mode);
void pd_mode_registry_run_unlock_hooks(pd_mode_mask_t mode);
