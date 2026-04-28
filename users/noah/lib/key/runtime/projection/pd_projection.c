#include "pd_projection.h"

#include "../reducer/ownership_state.h"

#include "../../../pointing/defs/pd_modes.h"
#include "../../../state/diagnostics/runtime_trace.h"
#include "../../ownership/held_action.h"

static keypos_t key_runtime_core_pd_projection_invalid_keypos(void) {
    return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static lease_kind_t key_runtime_core_pd_projection_lease_kind(const lease_t *lease) {
    return lease ? (lease_kind_t)lease->kind : LEASE_KIND_NONE;
}

static keypos_t key_runtime_core_pd_projection_lease_owner_key_pos(const lease_t *lease) {
    return lease ? key_runtime_keypos_unpack(lease->owner_packed_key_pos) : key_runtime_core_pd_projection_invalid_keypos();
}

static bool key_runtime_core_pd_projection_find_preempted_held_action(key_runtime_core_state_t *state, pd_mode_mask_t keep_mode, keypos_t *out_key_pos, uint16_t *out_action) {
    if (out_key_pos) {
        *out_key_pos = key_runtime_core_pd_projection_invalid_keypos();
    }
    if (out_action) {
        *out_action = KC_NO;
    }

    if (!(state && keep_mode != 0 && out_key_pos && out_action)) {
        return false;
    }

    for (uint16_t index = 0; index < KEY_RUNTIME_CORE_LEASE_CAPACITY; index++) {
        const lease_t  *lease = &state->leases[index];
        pd_mode_mask_t held_mode;

        if (!(lease->active && key_runtime_core_pd_projection_lease_kind(lease) == LEASE_KIND_HELD_ACTION)) {
            continue;
        }

        held_mode = pd_mode_for_keycode(lease->data.action);
        if (held_mode == 0 || held_mode == keep_mode) {
            continue;
        }

        *out_key_pos = key_runtime_core_pd_projection_lease_owner_key_pos(lease);
        *out_action  = lease->data.action;
        return true;
    }

    return false;
}

static void key_runtime_core_pd_projection_preempt_held_actions_for_mode(pd_mode_mask_t keep_mode) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    if (!(state && keep_mode != 0)) {
        return;
    }

    // PD mode state is exclusive. Keep held-action ownership exclusive too so
    // a preempted mode cannot leave a stale key-runtime owner behind.
    for (uint16_t guard = 0; guard < KEY_RUNTIME_CORE_LEASE_CAPACITY; guard++) {
        keypos_t preempted_key_pos;
        uint16_t preempted_action;

        if (!key_runtime_core_pd_projection_find_preempted_held_action(state, keep_mode, &preempted_key_pos, &preempted_action)) {
            return;
        }

        noah_runtime_trace_emit(NOAH_TRACE_KEY_RUNTIME, NOAH_TRACE_KEY_RUNTIME_EVENT_PD_HELD_PREEMPT, pd_mode_for_keycode(preempted_action), preempted_action);
        held_action_unregister(preempted_key_pos, preempted_action);
        key_runtime_core_observe_held_action_unregister(preempted_key_pos, preempted_action);
    }
}

static bool key_runtime_core_pd_projection_lock_tap_will_activate(pd_mode_mask_t mode) {
    return mode != 0 && (pd_mode_local_locked_snapshot() != mode || pd_mode_local_active_snapshot() != mode);
}

void key_runtime_core_pd_projection_preempt_held_action(uint16_t action) {
    key_runtime_core_pd_projection_preempt_held_actions_for_mode(pd_mode_for_keycode(action));
}

void key_runtime_core_pd_projection_project_lock_tap(pd_mode_mask_t mode, keypos_t key_pos) {
    if (key_runtime_core_pd_projection_lock_tap_will_activate(mode)) {
        key_runtime_core_pd_projection_preempt_held_actions_for_mode(mode);
    }

    (void)pd_mode_toggle_lock_state_at(mode, key_pos);
}
