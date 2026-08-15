// ────────────────────────────────────────────────────────────────────────────
// Split Runtime Sync Dirty State
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

void split_runtime_sync_dirty_reset(void);
bool split_runtime_sync_combo_is_dirty(void);
bool split_runtime_sync_key_feedback_semantic_is_dirty(void);
bool split_runtime_sync_key_feedback_branch_is_dirty(void);
void split_runtime_sync_clear_combo_dirty(void);
void split_runtime_sync_clear_key_feedback_semantic_dirty(void);
void split_runtime_sync_clear_key_feedback_branch_dirty(void);

#ifdef SPLIT_RUNTIME_SYNC_DIRTY_TEST_INSTRUMENTATION
void     split_runtime_sync_dirty_test_mark_count_reset(void);
uint16_t split_runtime_sync_dirty_test_mark_count(void);
#endif
