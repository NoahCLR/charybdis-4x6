// ────────────────────────────────────────────────────────────────────────────
// Runtime Reset
// ────────────────────────────────────────────────────────────────────────────
//
// Public hard-reset seam for the userspace-owned runtime state. Higher-level
// host tests should use this instead of rebuilding partial reset logic.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

void noah_runtime_reset_for_test(void);
