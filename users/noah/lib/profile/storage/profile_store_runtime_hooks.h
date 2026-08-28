// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Store Runtime Hooks
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>

void noah_profile_store_runtime_init(void);
void noah_profile_store_runtime_matrix_scan(void);
bool noah_profile_store_runtime_matrix_scan_step(void);
