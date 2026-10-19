// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Store Runtime Hooks
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

void noah_profile_store_runtime_init(void);
void noah_profile_store_runtime_matrix_scan(void);
bool noah_profile_store_runtime_matrix_scan_step(void);
// Once per durable-I/O grant: whether the store's step may not run in it (a
// save waiting for a quiet keyboard, or the grant's flash time spent). The
// owner's clock does not count those grants (D-F32).
void noah_profile_store_runtime_note_storage_paused(bool paused);
