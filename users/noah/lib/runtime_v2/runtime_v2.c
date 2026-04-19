// ────────────────────────────────────────────────────────────────────────────
// Runtime V2 Foundation
// ────────────────────────────────────────────────────────────────────────────

#include "runtime_v2.h"

#include <string.h>

#include "../pointing/policy/pointer_layer_policy.h"
#include "../state/ownership/keyboard_mod_ownership.h"
#include "../state/ownership/layer_ownership.h"
#include "../state/runtime/runtime_debug.h"
#include "runtime_v2_trace.h"

static uint8_t runtime_v2_refcount_mask(const uint8_t *refcounts, uint8_t count) {
    uint8_t mask = 0;

    if (!refcounts) {
        return 0;
    }

    for (uint8_t index = 0; index < count && index < 8u; index++) {
        if (refcounts[index] != 0u) {
            mask |= (uint8_t)(1u << index);
        }
    }

    return mask;
}

projection_snapshot_t runtime_v2_projection_snapshot_capture(void) {
    projection_snapshot_t                  snapshot = {0};
    layer_ownership_debug_snapshot_t       layer_snapshot;
    keyboard_mod_ownership_debug_snapshot_t mod_snapshot;
    pointer_layer_policy_debug_snapshot_t  pointer_snapshot;
    runtime_v2_state_t                    *state = runtime_v2_state();

    memset(&layer_snapshot, 0, sizeof(layer_snapshot));
    memset(&mod_snapshot, 0, sizeof(mod_snapshot));
    memset(&pointer_snapshot, 0, sizeof(pointer_snapshot));

    layer_ownership_debug_snapshot(&layer_snapshot);
    keyboard_mod_ownership_debug_snapshot(&mod_snapshot);
    pointer_layer_policy_debug_snapshot(layer_state, &pointer_snapshot);

    snapshot.layer_state                  = layer_state;
    snapshot.locked_layer_mask            = layer_snapshot.locked_mask;
    snapshot.keyboard_mod_state           = mod_snapshot.live_state;
    snapshot.keyboard_managed_mod_mask    = runtime_v2_refcount_mask(mod_snapshot.managed_refcounts, KEYBOARD_MOD_OWNERSHIP_MOD_COUNT);
    snapshot.keyboard_physical_mod_mask   = runtime_v2_refcount_mask(mod_snapshot.physical_refcounts, KEYBOARD_MOD_OWNERSHIP_MOD_COUNT);
    snapshot.pd_mode_local_active         = pd_mode_local_active_snapshot();
    snapshot.pd_mode_local_locked         = pd_mode_local_locked_snapshot();
    snapshot.pd_mode_display_active       = pd_mode_display_active_snapshot();
    snapshot.pd_mode_display_locked       = pd_mode_display_locked_snapshot();
    snapshot.pointer_anchor_active        = pointer_snapshot.auto_mouse_anchored;
    snapshot.pointer_pd_mode_anchor_active = pointer_snapshot.pd_mode_anchor_active;
    snapshot.pointer_prefers_typing_layer = pointer_snapshot.prefers_typing_layer;
    snapshot.pointer_toggle_enabled       = pointer_snapshot.auto_mouse_toggle_enabled;
    snapshot.pointer_sniping_layer_active = pointer_snapshot.sniping_layer_active;
    snapshot.pointer_key_tracker          = pointer_snapshot.auto_mouse_key_tracker;
    snapshot.pointer_layer                = pointer_snapshot.auto_mouse_layer;
    snapshot.active_slot_count            = noah_runtime_debug_active_slot_count();
    snapshot.pending_multi_tap_slot_count = noah_runtime_debug_pending_multi_tap_slot_count();
    snapshot.deferred_release_count       = noah_runtime_debug_deferred_release_count();

    if (state) {
        snapshot.v2_press_token_count     = state->press_token_count;
        snapshot.v2_tap_series_count      = state->tap_series_count;
        snapshot.v2_lease_count           = state->lease_count;
        snapshot.v2_persistent_intent_count = state->persistent_intent_count;
        state->last_projection            = snapshot;
    }

    return snapshot;
}

bool runtime_v2_projection_snapshot_equal(const projection_snapshot_t *lhs, const projection_snapshot_t *rhs) {
    if (!(lhs && rhs)) {
        return lhs == rhs;
    }

    return lhs->layer_state == rhs->layer_state &&
           lhs->locked_layer_mask == rhs->locked_layer_mask &&
           lhs->keyboard_mod_state.real == rhs->keyboard_mod_state.real &&
           lhs->keyboard_mod_state.weak == rhs->keyboard_mod_state.weak &&
           lhs->keyboard_mod_state.oneshot == rhs->keyboard_mod_state.oneshot &&
           lhs->keyboard_mod_state.oneshot_locked == rhs->keyboard_mod_state.oneshot_locked &&
           lhs->keyboard_managed_mod_mask == rhs->keyboard_managed_mod_mask &&
           lhs->keyboard_physical_mod_mask == rhs->keyboard_physical_mod_mask &&
           lhs->pd_mode_local_active == rhs->pd_mode_local_active &&
           lhs->pd_mode_local_locked == rhs->pd_mode_local_locked &&
           lhs->pd_mode_display_active == rhs->pd_mode_display_active &&
           lhs->pd_mode_display_locked == rhs->pd_mode_display_locked &&
           lhs->pointer_anchor_active == rhs->pointer_anchor_active &&
           lhs->pointer_pd_mode_anchor_active == rhs->pointer_pd_mode_anchor_active &&
           lhs->pointer_prefers_typing_layer == rhs->pointer_prefers_typing_layer &&
           lhs->pointer_toggle_enabled == rhs->pointer_toggle_enabled &&
           lhs->pointer_sniping_layer_active == rhs->pointer_sniping_layer_active &&
           lhs->pointer_key_tracker == rhs->pointer_key_tracker &&
           lhs->pointer_layer == rhs->pointer_layer &&
           lhs->active_slot_count == rhs->active_slot_count &&
           lhs->pending_multi_tap_slot_count == rhs->pending_multi_tap_slot_count &&
           lhs->deferred_release_count == rhs->deferred_release_count &&
           lhs->v2_press_token_count == rhs->v2_press_token_count &&
           lhs->v2_tap_series_count == rhs->v2_tap_series_count &&
           lhs->v2_lease_count == rhs->v2_lease_count &&
           lhs->v2_persistent_intent_count == rhs->v2_persistent_intent_count;
}

#if defined(NOAH_RUNTIME_TRACE_ENABLE)

void runtime_v2_trace_capture_projection(void) {
    projection_snapshot_t snapshot = runtime_v2_projection_snapshot_capture();

    runtime_v2_trace_record_projection_snapshot(&snapshot);
}

#endif // defined(NOAH_RUNTIME_TRACE_ENABLE)
