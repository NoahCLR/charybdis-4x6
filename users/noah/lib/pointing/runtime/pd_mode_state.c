// ────────────────────────────────────────────────────────────────────────────
// PD Mode State
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "pd_mode_runtime_shared_state_internal.h"
#include "../../key/runtime/slot/origin_registry.h"
#include "../../state/diagnostics/runtime_trace.h"
#include "../policy/pd_mode_policy.h"
#include "pd_mode_key_runtime_bridge.h"
#include "pd_mode_internal.h"

static pd_mode_runtime_shared_state_t *pd_mode_shared_state(void) {
    return pd_mode_runtime_shared_state();
}

#define PD_MODE_LOCAL_ACTIVE_MODE (pd_mode_shared_state()->local_active_mode)
#define PD_MODE_LOCAL_LOCKED_MODE (pd_mode_shared_state()->local_locked_mode)
#define PD_MODE_LOCAL_OWNER_KEY_POS_VALID (pd_mode_shared_state()->local_owner_key_pos_valid)
#define PD_MODE_LOCAL_OWNER_KEY_POS (pd_mode_shared_state()->local_owner_key_pos)
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
#    define PD_MODE_LOCAL_OWNER_SIDES (pd_mode_shared_state()->local_owner_sides)
#endif
#define PD_MODE_LOCAL_KEY_OWNERS (pd_mode_shared_state()->local_key_owners)

#ifdef PD_MODE_PUBLISH_TEST_BACKEND
static pd_mode_publish_seam_fn_t pd_mode_publish_seam = NULL;

void pd_mode_test_set_publish_seam(pd_mode_publish_seam_fn_t seam) {
    pd_mode_publish_seam = seam;
}

static void pd_mode_publish_seam_reached(void) {
    if (pd_mode_publish_seam) {
        pd_mode_publish_seam();
    }
}
#else
static void pd_mode_publish_seam_reached(void) {}
#endif

static bool pd_mode_snapshot_view_changed(pd_mode_snapshot_view_t before, pd_mode_snapshot_view_t after) {
    return before.active_mode != after.active_mode || before.locked_mode != after.locked_mode || before.owner_sides != after.owner_sides;
}

static split_side_mask_t pd_mode_command_owner_sides(pd_mode_command_t command) {
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    return command.owner_sides;
#else
    (void)command;
    return SPLIT_SIDE_MASK_NONE;
#endif
}

static keypos_t pd_mode_command_owner_key_pos(pd_mode_command_t command) {
    return command.owner_key_pos;
}

static const uint8_t *pd_mode_command_owner_bitmap(pd_mode_command_t command) {
    return command.owner_bitmap;
}

static keypos_t pd_mode_invalid_owner_key_pos(void) {
    return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static uint16_t pd_mode_trace_pack_keypos(keypos_t key_pos) {
    return (uint16_t)(((uint16_t)key_pos.row << 8) | key_pos.col);
}

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
static bool pd_mode_bitmap_equal(const uint8_t *lhs, const uint8_t *rhs) {
    if (!(lhs && rhs)) {
        return lhs == rhs;
    }

    for (uint8_t index = 0; index < KEY_ORIGIN_BITMAP_SIZE; index++) {
        if (lhs[index] != rhs[index]) {
            return false;
        }
    }

    return true;
}
#endif

static bool pd_mode_owner_bitmap_for_keypos(keypos_t owner_key_pos, uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return false;
    }

    key_origin_bitmap_clear(out_bitmap);
    if (!key_origin_keypos_valid(owner_key_pos)) {
        return false;
    }

    return key_origin_registry_get_bitmap(owner_key_pos, out_bitmap);
}

static bool pd_mode_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static bool pd_mode_sync_local_owner_state(keypos_t owner_key_pos, split_side_mask_t owner_sides) {
    bool owner_key_pos_valid = key_origin_keypos_valid(owner_key_pos);
    bool changed             = PD_MODE_LOCAL_OWNER_KEY_POS_VALID != owner_key_pos_valid;

    if (owner_key_pos_valid && !pd_mode_keypos_equal(PD_MODE_LOCAL_OWNER_KEY_POS, owner_key_pos)) {
        changed = true;
    }

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    changed |= PD_MODE_LOCAL_OWNER_SIDES != owner_sides;
#else
    (void)owner_sides;
#endif

    PD_MODE_LOCAL_OWNER_KEY_POS_VALID = owner_key_pos_valid;
    PD_MODE_LOCAL_OWNER_KEY_POS       = owner_key_pos_valid ? owner_key_pos : pd_mode_invalid_owner_key_pos();

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    PD_MODE_LOCAL_OWNER_SIDES = owner_sides;
#endif

    return changed;
}

static pd_mode_owner_slot_t *pd_mode_find_local_key_owner_slot(keypos_t owner_key_pos) {
    if (!key_origin_keypos_valid(owner_key_pos)) {
        return NULL;
    }

    for (uint16_t index = 0; index < PD_MODE_OWNER_SLOT_CAPACITY; index++) {
        pd_mode_owner_slot_t *slot = &PD_MODE_LOCAL_KEY_OWNERS[index];

        if (slot->active && pd_mode_keypos_equal(slot->key_pos, owner_key_pos)) {
            return slot;
        }
    }

    return NULL;
}

static pd_mode_owner_slot_t *pd_mode_find_free_local_key_owner_slot(void) {
    for (uint16_t index = 0; index < PD_MODE_OWNER_SLOT_CAPACITY; index++) {
        pd_mode_owner_slot_t *slot = &PD_MODE_LOCAL_KEY_OWNERS[index];

        if (!slot->active) {
            return slot;
        }
    }

    return NULL;
}

static bool pd_mode_clear_local_key_owners(void) {
    bool changed = false;

    for (uint16_t index = 0; index < PD_MODE_OWNER_SLOT_CAPACITY; index++) {
        if (PD_MODE_LOCAL_KEY_OWNERS[index].active) {
            noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_OWNER_CLEAR, PD_MODE_LOCAL_KEY_OWNERS[index].mode, pd_mode_trace_pack_keypos(PD_MODE_LOCAL_KEY_OWNERS[index].key_pos));
            PD_MODE_LOCAL_KEY_OWNERS[index] = (pd_mode_owner_slot_t){0};
            changed                         = true;
        }
    }

    return changed;
}

static bool pd_mode_local_key_owner_activate(pd_mode_mask_t mode, keypos_t owner_key_pos, split_side_mask_t owner_sides) {
    pd_mode_owner_slot_t *slot;

    if (!(mode != 0 && key_origin_keypos_valid(owner_key_pos))) {
        return false;
    }

    slot = pd_mode_find_local_key_owner_slot(owner_key_pos);
    if (!slot) {
        slot = pd_mode_find_free_local_key_owner_slot();
    }
    if (!slot) {
        return false;
    }

    if (slot->active && slot->mode == mode && pd_mode_keypos_equal(slot->key_pos, owner_key_pos) && slot->owner_sides == owner_sides) {
        return false;
    }

    *slot = (pd_mode_owner_slot_t){
        .active      = true,
        .mode        = mode,
        .key_pos     = owner_key_pos,
        .owner_sides = owner_sides,
    };
    noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_OWNER_PRESS, mode, pd_mode_trace_pack_keypos(owner_key_pos));
    return true;
}

static bool pd_mode_local_key_owner_release(pd_mode_mask_t mode, keypos_t owner_key_pos) {
    pd_mode_owner_slot_t *slot = pd_mode_find_local_key_owner_slot(owner_key_pos);

    if (!(slot && slot->mode == mode)) {
        return false;
    }

    noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_OWNER_RELEASE, mode, pd_mode_trace_pack_keypos(owner_key_pos));
    *slot = (pd_mode_owner_slot_t){0};
    return true;
}

static bool pd_mode_local_key_owner_mode_active(pd_mode_mask_t mode) {
    if (mode == 0) {
        return false;
    }

    for (uint16_t index = 0; index < PD_MODE_OWNER_SLOT_CAPACITY; index++) {
        const pd_mode_owner_slot_t *slot = &PD_MODE_LOCAL_KEY_OWNERS[index];

        if (slot->active && slot->mode == mode) {
            return true;
        }
    }

    return false;
}

static keypos_t pd_mode_local_key_owner_first_key_pos(pd_mode_mask_t mode) {
    if (mode == 0) {
        return pd_mode_invalid_owner_key_pos();
    }

    for (uint16_t index = 0; index < PD_MODE_OWNER_SLOT_CAPACITY; index++) {
        const pd_mode_owner_slot_t *slot = &PD_MODE_LOCAL_KEY_OWNERS[index];

        if (slot->active && slot->mode == mode) {
            return slot->key_pos;
        }
    }

    return pd_mode_invalid_owner_key_pos();
}

static split_side_mask_t pd_mode_local_key_owner_sides(pd_mode_mask_t mode) {
    split_side_mask_t owner_sides = SPLIT_SIDE_MASK_NONE;

    if (mode == 0) {
        return owner_sides;
    }

    for (uint16_t index = 0; index < PD_MODE_OWNER_SLOT_CAPACITY; index++) {
        const pd_mode_owner_slot_t *slot = &PD_MODE_LOCAL_KEY_OWNERS[index];

        if (slot->active && slot->mode == mode) {
            owner_sides |= slot->owner_sides;
        }
    }

    return owner_sides;
}

static bool pd_mode_local_key_owner_bitmap(pd_mode_mask_t mode, uint8_t *out_bitmap) {
    bool has_owner = false;

    if (!out_bitmap) {
        return false;
    }

    key_origin_bitmap_clear(out_bitmap);
    if (mode == 0) {
        return false;
    }

    for (uint16_t index = 0; index < PD_MODE_OWNER_SLOT_CAPACITY; index++) {
        const pd_mode_owner_slot_t *slot = &PD_MODE_LOCAL_KEY_OWNERS[index];
        uint8_t                     slot_bitmap[KEY_ORIGIN_BITMAP_SIZE];

        if (!(slot->active && slot->mode == mode)) {
            continue;
        }

        if (pd_mode_owner_bitmap_for_keypos(slot->key_pos, slot_bitmap)) {
            key_origin_bitmap_or_inplace(out_bitmap, slot_bitmap);
            has_owner = true;
        }
    }

    return has_owner;
}

static bool pd_mode_sync_local_key_owner_state(pd_mode_mask_t mode) {
    if (!pd_mode_local_key_owner_mode_active(mode)) {
        return pd_mode_sync_local_owner_state(pd_mode_invalid_owner_key_pos(), SPLIT_SIDE_MASK_NONE);
    }

    return pd_mode_sync_local_owner_state(pd_mode_local_key_owner_first_key_pos(mode), pd_mode_local_key_owner_sides(mode));
}

static bool pd_mode_apply_unlock_other_locks(pd_mode_mask_t keep_mode) {
    pd_mode_mask_t locked_mode = PD_MODE_LOCAL_LOCKED_MODE;

    if (locked_mode != 0 && locked_mode != keep_mode) {
        pd_mode_transition_unlock(locked_mode);
        return true;
    }

    return false;
}

static bool pd_mode_apply_deactivate_other_unlocked(pd_mode_mask_t keep_mode) {
    pd_mode_mask_t active_mode = PD_MODE_LOCAL_ACTIVE_MODE;

    if (active_mode != 0 && active_mode != keep_mode && active_mode != PD_MODE_LOCAL_LOCKED_MODE) {
        pd_mode_transition_deactivate(active_mode);
        return true;
    }

    return false;
}

static bool pd_mode_apply_activate_mode(pd_mode_mask_t mode) {
    bool changed = false;

    if (!mode) {
        return false;
    }

    changed |= pd_mode_apply_unlock_other_locks(mode);
    changed |= pd_mode_apply_deactivate_other_unlocked(mode);

    if (!pd_mode_local_active(mode)) {
        pd_mode_transition_activate(mode);
        changed = true;
    }

    return changed;
}

static bool pd_mode_apply_deactivate_mode(pd_mode_mask_t mode) {
    if (!mode || !pd_mode_local_active(mode)) {
        return false;
    }

    pd_mode_transition_deactivate(mode);
    return true;
}

static bool pd_mode_apply_lock_mode(pd_mode_mask_t mode) {
    bool changed = false;

    if (!mode) {
        return false;
    }

    changed |= pd_mode_apply_unlock_other_locks(mode);
    changed |= pd_mode_apply_deactivate_other_unlocked(mode);

    if (!pd_mode_local_locked(mode) || !pd_mode_local_active(mode)) {
        pd_mode_transition_lock(mode);
        changed = true;
    }

    return changed;
}

static bool pd_mode_apply_unlock_mode(pd_mode_mask_t mode) {
    if (!mode || !pd_mode_local_locked(mode)) {
        return false;
    }

    pd_mode_transition_unlock(mode);
    return true;
}

static bool pd_mode_apply_remote_display_snapshot(pd_mode_mask_t active_mode, pd_mode_mask_t locked_mode, split_side_mask_t owner_sides, const uint8_t *owner_bitmap) {
    // Remote sync only mirrors mode state for the non-master half's policy/UI.
    // Do not replay local side effects such as dragscroll or auto-mouse
    // ownership changes from this path. Keep only one effective mode so the
    // mirrored UI matches the local exclusivity invariant.
    pd_mode_runtime_shared_state_t       *state     = pd_mode_shared_state();
    const pd_mode_remote_display_state_t *published = pd_mode_remote_display_published(state);
    pd_mode_remote_display_state_t       *pending   = pd_mode_remote_display_pending(state);
    bool                                  changed   = published->active_mode != active_mode || published->locked_mode != locked_mode;

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    changed |= published->owner_sides != owner_sides;
    if (owner_bitmap) {
        changed |= !pd_mode_bitmap_equal(published->owner_bitmap, owner_bitmap);
    } else {
        changed |= key_origin_bitmap_has_any(published->owner_bitmap);
    }
#else
    (void)owner_sides;
    (void)owner_bitmap;
#endif

    pending->locked_mode = locked_mode;
    pending->active_mode = active_mode;
    pd_mode_publish_seam_reached();
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    pending->owner_sides = owner_sides;
    if (owner_bitmap) {
        key_origin_bitmap_copy(pending->owner_bitmap, owner_bitmap);
    } else {
        key_origin_bitmap_clear(pending->owner_bitmap);
    }
#endif
    noah_runtime_publication_publish(&state->remote_display_generation);

    noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_REMOTE_SNAPSHOT, active_mode, locked_mode);
    return changed;
}

static pd_mode_apply_result_t pd_mode_apply_result_begin(void) {
    return (pd_mode_apply_result_t){
        .before = pd_mode_snapshot(),
    };
}

static void pd_mode_apply_result_finish(pd_mode_apply_result_t *result, bool split_sync_required) {
    if (!result) {
        return;
    }

    result->after                 = pd_mode_snapshot();
    result->local_state_changed   = pd_mode_snapshot_view_changed(result->before.local, result->after.local);
    result->display_state_changed = pd_mode_snapshot_view_changed(result->before.display, result->after.display);
    result->split_sync_required   = split_sync_required;
}

void pd_mode_set(pd_mode_mask_t mode) {
    PD_MODE_LOCAL_ACTIVE_MODE = mode;
}

void pd_mode_clear(pd_mode_mask_t mode) {
    if (PD_MODE_LOCAL_ACTIVE_MODE == mode) {
        PD_MODE_LOCAL_ACTIVE_MODE = 0;
    }
}

void pd_mode_set_locked(pd_mode_mask_t mode) {
    PD_MODE_LOCAL_LOCKED_MODE = mode;
}

void pd_mode_clear_locked(pd_mode_mask_t mode) {
    if (PD_MODE_LOCAL_LOCKED_MODE == mode) {
        PD_MODE_LOCAL_LOCKED_MODE = 0;
    }
}

pd_mode_mask_t pd_mode_local_active_snapshot(void) {
    return PD_MODE_LOCAL_ACTIVE_MODE;
}

pd_mode_mask_t pd_mode_local_locked_snapshot(void) {
    return PD_MODE_LOCAL_LOCKED_MODE;
}

pd_mode_mask_t pd_mode_display_active_snapshot(void) {
    return pd_mode_snapshot().display.active_mode;
}

pd_mode_mask_t pd_mode_display_locked_snapshot(void) {
    return pd_mode_snapshot().display.locked_mode;
}

split_side_mask_t pd_mode_local_owner_sides_snapshot(void) {
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    return PD_MODE_LOCAL_OWNER_SIDES;
#else
    return SPLIT_SIDE_MASK_NONE;
#endif
}

split_side_mask_t pd_mode_display_owner_sides_snapshot(void) {
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    if (is_keyboard_master()) {
        return PD_MODE_LOCAL_OWNER_SIDES;
    }

    return pd_mode_remote_display_snapshot(pd_mode_shared_state()).owner_sides;
#else
    return SPLIT_SIDE_MASK_NONE;
#endif
}

bool pd_mode_local_owner_key_pos_snapshot(keypos_t *out) {
    if (out) {
        *out = (keypos_t){0};
    }

    if (!(out && PD_MODE_LOCAL_OWNER_KEY_POS_VALID)) {
        return false;
    }

    *out = PD_MODE_LOCAL_OWNER_KEY_POS;
    return true;
}

bool pd_mode_local_owner_bitmap_snapshot(uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return false;
    }

    key_origin_bitmap_clear(out_bitmap);
    if (pd_mode_local_key_owner_bitmap(PD_MODE_LOCAL_ACTIVE_MODE, out_bitmap)) {
        return true;
    }

    if (!PD_MODE_LOCAL_OWNER_KEY_POS_VALID) {
        return false;
    }

    return pd_mode_owner_bitmap_for_keypos(PD_MODE_LOCAL_OWNER_KEY_POS, out_bitmap);
}

bool pd_mode_display_owner_bitmap_snapshot(uint8_t *out_bitmap) {
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    pd_mode_remote_display_state_t display;
#endif

    if (!out_bitmap) {
        return false;
    }

    key_origin_bitmap_clear(out_bitmap);
    if (is_keyboard_master()) {
        return pd_mode_local_owner_bitmap_snapshot(out_bitmap);
    }

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    display = pd_mode_remote_display_snapshot(pd_mode_shared_state());
    key_origin_bitmap_copy(out_bitmap, display.owner_bitmap);
    return key_origin_bitmap_has_any(out_bitmap);
#else
    return false;
#endif
}

bool pd_mode_local_active(pd_mode_mask_t mode) {
    return mode != 0 && PD_MODE_LOCAL_ACTIVE_MODE == mode;
}

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    return mode != 0 && PD_MODE_LOCAL_LOCKED_MODE == mode;
}

bool pd_mode_display_active(pd_mode_mask_t mode) {
    return mode != 0 && pd_mode_snapshot().display.active_mode == mode;
}

bool pd_mode_display_locked(pd_mode_mask_t mode) {
    return mode != 0 && pd_mode_snapshot().display.locked_mode == mode;
}

bool pd_any_local_mode_active(void) {
    return PD_MODE_LOCAL_ACTIVE_MODE != 0;
}

bool pd_any_local_mode_locked(void) {
    return PD_MODE_LOCAL_LOCKED_MODE != 0;
}

bool pd_any_display_mode_locked(void) {
    return pd_mode_snapshot().display.locked_mode != 0;
}

pd_mode_apply_result_t pd_mode_apply_command(pd_mode_command_t command) {
    pd_mode_apply_result_t result              = pd_mode_apply_result_begin();
    bool                   split_sync_required = false;
    pd_mode_mask_t         mode                = command.mode;

    switch (command.kind) {
        case PD_MODE_COMMAND_ACTIVATE:
            result.handled      = mode != 0;
            split_sync_required = pd_mode_apply_activate_mode(mode);
            if (split_sync_required) {
                (void)pd_mode_clear_local_key_owners();
                split_sync_required |= pd_mode_sync_local_owner_state(pd_mode_command_owner_key_pos(command), pd_mode_command_owner_sides(command));
            }
            break;
        case PD_MODE_COMMAND_DEACTIVATE:
            result.handled      = mode != 0;
            split_sync_required = pd_mode_apply_deactivate_mode(mode);
            if (split_sync_required) {
                (void)pd_mode_clear_local_key_owners();
                split_sync_required |= pd_mode_sync_local_owner_state(pd_mode_invalid_owner_key_pos(), SPLIT_SIDE_MASK_NONE);
            }
            break;
        case PD_MODE_COMMAND_LOCK:
            result.handled      = mode != 0;
            split_sync_required = pd_mode_apply_lock_mode(mode);
            if (split_sync_required) {
                (void)pd_mode_clear_local_key_owners();
                split_sync_required |= pd_mode_sync_local_owner_state(pd_mode_command_owner_key_pos(command), pd_mode_command_owner_sides(command));
            }
            break;
        case PD_MODE_COMMAND_UNLOCK:
            result.handled      = mode != 0;
            split_sync_required = pd_mode_apply_unlock_mode(mode);
            if (split_sync_required) {
                (void)pd_mode_clear_local_key_owners();
                split_sync_required |= pd_mode_sync_local_owner_state(pd_mode_invalid_owner_key_pos(), SPLIT_SIDE_MASK_NONE);
            }
            break;
        case PD_MODE_COMMAND_KEY_PRESS:
            mode           = pd_mode_for_keycode(command.keycode);
            result.handled = mode != 0;
            if (mode != 0) {
                bool locked_same_mode = pd_mode_local_locked(mode);
                bool owner_changed    = false;

                noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_KEY_PRESS, mode, pd_mode_trace_pack_keypos(pd_mode_command_owner_key_pos(command)));
                split_sync_required = pd_mode_apply_activate_mode(mode);
                if (!locked_same_mode) {
                    if (split_sync_required) {
                        (void)pd_mode_clear_local_key_owners();
                    }
                    owner_changed |= pd_mode_local_key_owner_activate(mode, pd_mode_command_owner_key_pos(command), pd_mode_command_owner_sides(command));
                    if (split_sync_required || owner_changed) {
                        owner_changed |= pd_mode_sync_local_key_owner_state(mode);
                    }
                }
                split_sync_required |= owner_changed;
            }
            break;
        case PD_MODE_COMMAND_KEY_RELEASE:
            mode           = pd_mode_for_keycode(command.keycode);
            result.handled = mode != 0;
            if (mode != 0) {
                noah_runtime_trace_emit(NOAH_TRACE_PD_MODE, NOAH_TRACE_PD_MODE_EVENT_KEY_RELEASE, mode, pd_mode_trace_pack_keypos(pd_mode_command_owner_key_pos(command)));
            }
            if (mode != 0 && pd_mode_local_active(mode) && !pd_mode_local_locked(mode)) {
                bool owner_changed = pd_mode_local_key_owner_release(mode, pd_mode_command_owner_key_pos(command));

                if (pd_mode_local_key_owner_mode_active(mode)) {
                    owner_changed |= pd_mode_sync_local_key_owner_state(mode);
                    split_sync_required = owner_changed;
                } else {
                    split_sync_required = pd_mode_apply_deactivate_mode(mode);
                    if (split_sync_required || owner_changed) {
                        split_sync_required |= pd_mode_sync_local_owner_state(pd_mode_invalid_owner_key_pos(), SPLIT_SIDE_MASK_NONE);
                    }
                }
            }
            break;
        case PD_MODE_COMMAND_REMOTE_SNAPSHOT:
            result.handled = true;
            (void)pd_mode_apply_remote_display_snapshot(pd_mode_mask_from_id(command.active_mode_id), pd_mode_mask_from_id(command.locked_mode_id), command.owner_sides, pd_mode_command_owner_bitmap(command));
            break;
        case PD_MODE_COMMAND_NONE:
        default:
            break;
    }

    pd_mode_apply_result_finish(&result, split_sync_required);
    return result;
}

void pd_mode_activate(pd_mode_mask_t mode) {
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind          = PD_MODE_COMMAND_ACTIVATE,
        .mode          = mode,
        .owner_key_pos = {.row = MATRIX_ROWS, .col = MATRIX_COLS},
    });
}

void pd_mode_deactivate(pd_mode_mask_t mode) {
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind          = PD_MODE_COMMAND_DEACTIVATE,
        .mode          = mode,
        .owner_key_pos = {.row = MATRIX_ROWS, .col = MATRIX_COLS},
    });
}

void pd_mode_apply_remote_mode_ids(pd_mode_id_t active_mode_id, pd_mode_id_t locked_mode_id, split_side_mask_t owner_sides) {
    pd_mode_apply_remote_mode_ids_with_owner_bitmap(active_mode_id, locked_mode_id, owner_sides, NULL);
}

void pd_mode_apply_remote_mode_ids_with_owner_bitmap(pd_mode_id_t active_mode_id, pd_mode_id_t locked_mode_id, split_side_mask_t owner_sides, const uint8_t *owner_bitmap) {
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind           = PD_MODE_COMMAND_REMOTE_SNAPSHOT,
        .active_mode_id = active_mode_id,
        .locked_mode_id = locked_mode_id,
        .owner_sides    = owner_sides,
        .owner_key_pos  = {.row = MATRIX_ROWS, .col = MATRIX_COLS},
        .owner_bitmap   = owner_bitmap,
    });
}

void pd_mode_apply_remote_snapshot(pd_mode_mask_t active_flags, pd_mode_mask_t locked_flags) {
    pd_mode_mask_t locked_mode = locked_flags != 0 ? locked_flags : 0;
    pd_mode_mask_t active_mode = locked_mode != 0 ? locked_mode : active_flags;

    pd_mode_apply_remote_mode_ids(pd_mode_id_from_mask(active_mode), pd_mode_id_from_mask(locked_mode), SPLIT_SIDE_MASK_NONE);
}

bool pd_mode_set_lock_state(pd_mode_mask_t mode, bool locked) {
    return pd_mode_set_lock_state_at(mode, locked, (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS});
}

static bool pd_mode_apply_local_lock_state_at(pd_mode_mask_t mode, bool locked, keypos_t key_pos) {
    pd_mode_apply_result_t result = pd_mode_apply_command((pd_mode_command_t){
        .kind          = locked ? PD_MODE_COMMAND_LOCK : PD_MODE_COMMAND_UNLOCK,
        .mode          = mode,
        .owner_sides   = key_origin_registry_side_mask(key_pos),
        .owner_key_pos = key_pos,
    });

    if (result.local_state_changed) {
        pd_mode_key_runtime_bridge_observe_local_lock_state(mode, locked);
    }

    return result.local_state_changed;
}

bool pd_mode_set_lock_state_at(pd_mode_mask_t mode, bool locked, keypos_t key_pos) {
    return pd_mode_apply_local_lock_state_at(mode, locked, key_pos);
}

void pd_mode_lock(pd_mode_mask_t mode) {
    (void)pd_mode_apply_local_lock_state_at(mode, true, pd_mode_invalid_owner_key_pos());
}

void pd_mode_unlock(pd_mode_mask_t mode) {
    (void)pd_mode_apply_local_lock_state_at(mode, false, pd_mode_invalid_owner_key_pos());
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    return pd_mode_toggle_lock_state_at(mode, (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS});
}

bool pd_mode_toggle_lock_state_at(pd_mode_mask_t mode, keypos_t key_pos) {
    return pd_mode_set_lock_state_at(mode, !pd_mode_local_locked(mode), key_pos);
}

bool pd_mode_handle_keycode_press(uint16_t keycode) {
    return pd_mode_handle_keycode_press_at(keycode, (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS});
}

bool pd_mode_handle_keycode_press_at(uint16_t keycode, keypos_t key_pos) {
    pd_mode_apply_result_t result = pd_mode_apply_command((pd_mode_command_t){
        .kind          = PD_MODE_COMMAND_KEY_PRESS,
        .keycode       = keycode,
        .owner_sides   = key_origin_registry_side_mask(key_pos),
        .owner_key_pos = key_pos,
    });

    return result.handled;
}

bool pd_mode_handle_keycode_release(uint16_t keycode) {
    return pd_mode_handle_keycode_release_at(keycode, (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS});
}

bool pd_mode_handle_keycode_release_at(uint16_t keycode, keypos_t key_pos) {
    pd_mode_apply_result_t result = pd_mode_apply_command((pd_mode_command_t){
        .kind          = PD_MODE_COMMAND_KEY_RELEASE,
        .keycode       = keycode,
        .owner_sides   = key_origin_registry_side_mask(key_pos),
        .owner_key_pos = key_pos,
    });

    return result.handled;
}

#undef PD_MODE_LOCAL_ACTIVE_MODE
#undef PD_MODE_LOCAL_LOCKED_MODE
#undef PD_MODE_LOCAL_OWNER_KEY_POS_VALID
#undef PD_MODE_LOCAL_OWNER_KEY_POS
#undef PD_MODE_LOCAL_KEY_OWNERS
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
#    undef PD_MODE_LOCAL_OWNER_SIDES
#endif
