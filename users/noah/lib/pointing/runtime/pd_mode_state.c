// ────────────────────────────────────────────────────────────────────────────
// PD Mode State
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../state/runtime/runtime_shared_state.h"
#include "../../state/runtime/split_runtime_sync.h"
#include "../../state/runtime/runtime_trace.h"
#include "pd_mode_internal.h"

#define PD_MODE_LOCAL_ACTIVE_MODE (noah_runtime_shared_state.pd.local_active_mode)
#define PD_MODE_LOCAL_LOCKED_MODE (noah_runtime_shared_state.pd.local_locked_mode)
#define PD_MODE_REMOTE_DISPLAY_ACTIVE_MODE (noah_runtime_shared_state.pd.remote_display_active_mode)
#define PD_MODE_REMOTE_DISPLAY_LOCKED_MODE (noah_runtime_shared_state.pd.remote_display_locked_mode)

static pd_mode_mask_t pd_mode_first_snapshot_match(pd_mode_mask_t flags) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if ((flags & pd_modes[i].mode_flag) != 0) {
            return pd_modes[i].mode_flag;
        }
    }

    return 0;
}

static bool pd_mode_snapshot_view_changed(pd_mode_snapshot_view_t before, pd_mode_snapshot_view_t after) {
    return before.active_mode != after.active_mode || before.locked_mode != after.locked_mode;
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

static bool pd_mode_apply_remote_display_snapshot(pd_mode_mask_t active_flags, pd_mode_mask_t locked_flags) {
    // Remote sync only mirrors mode state for the non-master half's policy/UI.
    // Do not replay local side effects such as dragscroll or auto-mouse
    // ownership changes from this path. Keep only one effective mode so the
    // mirrored UI matches the local exclusivity invariant.
    pd_mode_mask_t locked_mode = pd_mode_first_snapshot_match(locked_flags);
    pd_mode_mask_t active_mode = locked_mode ? locked_mode : pd_mode_first_snapshot_match(active_flags);
    bool           changed     = PD_MODE_REMOTE_DISPLAY_ACTIVE_MODE != active_mode || PD_MODE_REMOTE_DISPLAY_LOCKED_MODE != locked_mode;

    PD_MODE_REMOTE_DISPLAY_LOCKED_MODE = locked_mode;
    PD_MODE_REMOTE_DISPLAY_ACTIVE_MODE = active_mode;
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

    result->after = pd_mode_snapshot();
    result->local_state_changed = pd_mode_snapshot_view_changed(result->before.local, result->after.local);
    result->display_state_changed = pd_mode_snapshot_view_changed(result->before.display, result->after.display);
    result->split_sync_required = split_sync_required && result->local_state_changed;
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

bool pd_any_display_mode_active(void) {
    return pd_mode_snapshot().display.active_mode != 0;
}

bool pd_any_display_mode_locked(void) {
    return pd_mode_snapshot().display.locked_mode != 0;
}

pd_mode_apply_result_t pd_mode_apply_command(pd_mode_command_t command) {
    pd_mode_apply_result_t result              = pd_mode_apply_result_begin();
    bool                   split_sync_required = false;
    pd_mode_mask_t         mode               = command.mode;

    switch (command.kind) {
        case PD_MODE_COMMAND_ACTIVATE:
            result.handled      = mode != 0;
            split_sync_required = pd_mode_apply_activate_mode(mode);
            break;
        case PD_MODE_COMMAND_DEACTIVATE:
            result.handled      = mode != 0;
            split_sync_required = pd_mode_apply_deactivate_mode(mode);
            break;
        case PD_MODE_COMMAND_LOCK:
            result.handled      = mode != 0;
            split_sync_required = pd_mode_apply_lock_mode(mode);
            break;
        case PD_MODE_COMMAND_UNLOCK:
            result.handled      = mode != 0;
            split_sync_required = pd_mode_apply_unlock_mode(mode);
            break;
        case PD_MODE_COMMAND_KEY_PRESS:
            mode           = pd_mode_for_keycode(command.keycode);
            result.handled = mode != 0;
            if (mode != 0) {
                split_sync_required = pd_mode_apply_activate_mode(mode);
            }
            break;
        case PD_MODE_COMMAND_KEY_RELEASE:
            mode           = pd_mode_for_keycode(command.keycode);
            result.handled = mode != 0;
            if (mode != 0 && pd_mode_local_active(mode) && !pd_mode_local_locked(mode)) {
                split_sync_required = pd_mode_apply_deactivate_mode(mode);
            }
            break;
        case PD_MODE_COMMAND_REMOTE_SNAPSHOT:
            result.handled = true;
            (void)pd_mode_apply_remote_display_snapshot(command.active_flags, command.locked_flags);
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
        .kind = PD_MODE_COMMAND_ACTIVATE,
        .mode = mode,
    });
}

void pd_mode_deactivate(pd_mode_mask_t mode) {
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind = PD_MODE_COMMAND_DEACTIVATE,
        .mode = mode,
    });
}

void pd_mode_lock(pd_mode_mask_t mode) {
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind = PD_MODE_COMMAND_LOCK,
        .mode = mode,
    });
}

void pd_mode_unlock(pd_mode_mask_t mode) {
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind = PD_MODE_COMMAND_UNLOCK,
        .mode = mode,
    });
}

void pd_mode_apply_remote_snapshot(pd_mode_mask_t active_flags, pd_mode_mask_t locked_flags) {
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind         = PD_MODE_COMMAND_REMOTE_SNAPSHOT,
        .active_flags = active_flags,
        .locked_flags = locked_flags,
    });
}

bool pd_mode_set_lock_state(pd_mode_mask_t mode, bool locked) {
    pd_mode_apply_result_t result = pd_mode_apply_command((pd_mode_command_t){
        .kind = locked ? PD_MODE_COMMAND_LOCK : PD_MODE_COMMAND_UNLOCK,
        .mode = mode,
    });

    if (result.split_sync_required) {
        split_runtime_sync();
    }

    return result.local_state_changed;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    return pd_mode_set_lock_state(mode, !pd_mode_local_locked(mode));
}

bool pd_mode_handle_keycode_press(uint16_t keycode) {
    pd_mode_apply_result_t result = pd_mode_apply_command((pd_mode_command_t){
        .kind    = PD_MODE_COMMAND_KEY_PRESS,
        .keycode = keycode,
    });

    if (result.split_sync_required) {
        split_runtime_sync();
    }

    return result.handled;
}

bool pd_mode_handle_keycode_release(uint16_t keycode) {
    pd_mode_apply_result_t result = pd_mode_apply_command((pd_mode_command_t){
        .kind    = PD_MODE_COMMAND_KEY_RELEASE,
        .keycode = keycode,
    });

    if (result.split_sync_required) {
        split_runtime_sync();
    }

    return result.handled;
}

#undef PD_MODE_LOCAL_ACTIVE_FLAGS
#undef PD_MODE_LOCAL_LOCKED_FLAGS
#undef PD_MODE_LOCAL_ACTIVE_MODE
#undef PD_MODE_LOCAL_LOCKED_MODE
#undef PD_MODE_REMOTE_DISPLAY_ACTIVE_MODE
#undef PD_MODE_REMOTE_DISPLAY_LOCKED_MODE
