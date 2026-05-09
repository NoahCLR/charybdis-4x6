// ────────────────────────────────────────────────────────────────────────────
// Split Runtime Sync Dirty State
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>

void split_runtime_sync_dirty_reset(void);
bool split_runtime_sync_combo_is_dirty(void);
bool split_runtime_sync_key_feedback_semantic_is_dirty(void);
bool split_runtime_sync_key_feedback_branch_is_dirty(void);
void split_runtime_sync_clear_combo_dirty(void);
void split_runtime_sync_clear_key_feedback_semantic_dirty(void);
void split_runtime_sync_clear_key_feedback_branch_dirty(void);
