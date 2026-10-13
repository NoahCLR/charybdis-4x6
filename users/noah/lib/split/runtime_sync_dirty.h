// ────────────────────────────────────────────────────────────────────────────
// Split Runtime Sync Dirty State
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Combo and key-feedback changes mark the lighting facts frame dirty, so its
// gated slices are rebuilt on the next pass.
void split_runtime_sync_dirty_reset(void);
bool split_runtime_sync_lighting_facts_is_dirty(void);
void split_runtime_sync_clear_lighting_facts_dirty(void);

#ifdef SPLIT_RUNTIME_SYNC_DIRTY_TEST_INSTRUMENTATION
void     split_runtime_sync_dirty_test_mark_count_reset(void);
uint16_t split_runtime_sync_dirty_test_mark_count(void);
#endif
