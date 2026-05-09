// ────────────────────────────────────────────────────────────────────────────
// Split Runtime Sync Dirty State
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(SPLIT_TRANSACTION_IDS_USER)

#    include "runtime_sync.h"
#    include "runtime_sync_dirty.h"

static bool split_runtime_combo_dirty                 = true;
static bool split_runtime_key_feedback_semantic_dirty = true;
static bool split_runtime_key_feedback_branch_dirty   = true;

void split_runtime_sync_dirty_reset(void) {
    split_runtime_combo_dirty                 = true;
    split_runtime_key_feedback_semantic_dirty = true;
    split_runtime_key_feedback_branch_dirty   = true;
}

void split_runtime_sync_mark_combo_dirty(void) {
    split_runtime_combo_dirty = true;
}

void split_runtime_sync_mark_key_feedback_dirty(void) {
    split_runtime_key_feedback_semantic_dirty = true;
    split_runtime_key_feedback_branch_dirty   = true;
}

bool split_runtime_sync_combo_is_dirty(void) {
    return split_runtime_combo_dirty;
}

bool split_runtime_sync_key_feedback_semantic_is_dirty(void) {
    return split_runtime_key_feedback_semantic_dirty;
}

bool split_runtime_sync_key_feedback_branch_is_dirty(void) {
    return split_runtime_key_feedback_branch_dirty;
}

void split_runtime_sync_clear_combo_dirty(void) {
    split_runtime_combo_dirty = false;
}

void split_runtime_sync_clear_key_feedback_semantic_dirty(void) {
    split_runtime_key_feedback_semantic_dirty = false;
}

void split_runtime_sync_clear_key_feedback_branch_dirty(void) {
    split_runtime_key_feedback_branch_dirty = false;
}

#endif // defined(SPLIT_TRANSACTION_IDS_USER)
