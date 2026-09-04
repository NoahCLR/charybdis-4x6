// ────────────────────────────────────────────────────────────────────────────
// Runtime Reset
// ────────────────────────────────────────────────────────────────────────────
//
// Public hard-reset seam for the userspace-owned runtime state. Higher-level
// host tests should use this instead of rebuilding partial reset logic.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

void noah_runtime_reset_for_test(void);

// Applies the non-zero key runtime core defaults at boot. The context itself
// is zero-initialised, so this must run before any stage that can observe key
// runtime core state.
void noah_runtime_shared_state_post_init(void);
