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
// Whether the store has local work in progress that more steps in the same
// grant can move on (noah_profile_owner_storage_busy).
bool noah_profile_store_runtime_storage_busy(void);
// Whether the next step repeats the last one within the same grant
// (noah_profile_owner_scan_again).
void noah_profile_store_runtime_scan_again(bool again);
