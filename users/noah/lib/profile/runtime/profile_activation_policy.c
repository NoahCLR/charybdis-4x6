// ───────────────────────────────────────────────────────────────────────────
// Live-Profile Activation Safety Policy
// ───────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "profile_activation_policy.h"

#include <limits.h>

#include "../../action/owned_keycode.h"
#include "../../compat/qmk_combo_origin.h"
#include "../../key/runtime/api.h"
#include "../../macro/macro_payload.h"

static void capture_runtime(noah_profile_activation_snapshot_t *snapshot) {
    noah_key_runtime_activity_snapshot_t   key_runtime = {0};
    macro_payload_debug_snapshot_t         macro       = {0};
    noah_qmk_combo_origin_debug_snapshot_t combo       = {0};

    if (!snapshot) {
        return;
    }

    noah_key_runtime_activity_snapshot(&key_runtime);
    macro_payload_debug_snapshot(&macro);
    noah_qmk_combo_origin_debug_snapshot(&combo);

    *snapshot = (noah_profile_activation_snapshot_t){
        .physical_press_count    = key_runtime.press_token_count,
        .tap_series_count        = key_runtime.tap_series_count,
        .runtime_lease_count     = key_runtime.lease_count,
        .deferred_release_count  = key_runtime.pending_release_count,
        .persistent_intent_count = key_runtime.persistent_intent_count,
        .owned_output_count      = owned_keycode_managed_usage_count(),
        .real_mods               = get_mods(),
        .weak_mods               = get_weak_mods(),
#ifndef NO_ACTION_ONESHOT
        .oneshot_mods         = get_oneshot_mods(),
        .oneshot_locked_mods  = get_oneshot_locked_mods(),
        .oneshot_layer_active = is_oneshot_layer_active() ? 1u : 0u,
#endif
        .macro_engine_state  = (uint8_t)macro.state,
        .macro_hold_count    = macro.active_hold_count,
        .combo_pending_count = combo.pending_count,
        .combo_active_count  = combo.active_count,
    };
}

void noah_profile_activation_policy_init(noah_profile_activation_policy_t *policy, noah_profile_activation_peer_observer_fn peer_observer, void *peer_context) {
    if (!policy) {
        return;
    }

    *policy = (noah_profile_activation_policy_t){
        .peer_observer = peer_observer,
        .peer_context  = peer_context,
        .initialized   = true,
    };
}

uint32_t noah_profile_activation_reason_mask(const noah_profile_activation_snapshot_t *snapshot) {
    uint32_t reason_mask = 0u;

    if (!snapshot) {
        return NOAH_PROFILE_ACTIVATION_REASON_INTERNAL;
    }
    if (snapshot->physical_press_count != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_PHYSICAL_PRESS;
    }
    if (snapshot->tap_series_count != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_TAP_SERIES;
    }
    if (snapshot->runtime_lease_count != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_RUNTIME_LEASE;
    }
    if (snapshot->deferred_release_count != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_DEFERRED_RELEASE;
    }
    if (snapshot->persistent_intent_count != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT;
    }
    if (snapshot->owned_output_count != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_OWNED_OUTPUT;
    }
    if ((uint8_t)(snapshot->real_mods | snapshot->weak_mods | snapshot->oneshot_mods | snapshot->oneshot_locked_mods) != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_MODIFIER_OR_ONESHOT;
    }
    if (snapshot->oneshot_layer_active != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_ONESHOT_LAYER;
    }
    if (snapshot->macro_engine_state != 0u || snapshot->macro_hold_count != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_MACRO;
    }
    if (snapshot->combo_pending_count != 0u || snapshot->combo_active_count != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_COMBO;
    }
    if (snapshot->unresolved_peer_count != 0u) {
        reason_mask |= NOAH_PROFILE_ACTIVATION_REASON_PEER;
    }
    return reason_mask;
}

uint32_t noah_profile_activation_policy_safe_boundary(void *context) {
    noah_profile_activation_policy_t  *policy = context;
    noah_profile_activation_snapshot_t snapshot;
    uint32_t                           reason_mask;
    uint32_t                           evaluation_count;
    uint8_t                            unresolved_peer_count = 1u;

    if (!(policy && policy->initialized)) {
        return NOAH_PROFILE_ACTIVATION_REASON_INTERNAL;
    }

    capture_runtime(&snapshot);
    if (policy->peer_observer && !policy->peer_observer(policy->peer_context, &unresolved_peer_count)) {
        unresolved_peer_count = 1u;
    }
    snapshot.unresolved_peer_count = unresolved_peer_count;
    reason_mask                    = noah_profile_activation_reason_mask(&snapshot);
    evaluation_count               = policy->evaluation_count;
    if (evaluation_count != UINT32_MAX) {
        evaluation_count++;
    }

    noah_runtime_publication_begin(&policy->publication_sequence);
    policy->last_snapshot    = snapshot;
    policy->last_reason_mask = reason_mask;
    policy->evaluation_count = evaluation_count;
    noah_runtime_publication_end(&policy->publication_sequence);
    return reason_mask;
}

bool noah_profile_activation_policy_status(const noah_profile_activation_policy_t *policy, noah_profile_activation_snapshot_t *snapshot, uint32_t *reason_mask, uint32_t *evaluation_count) {
    uint8_t attempt;

    if (!(policy && policy->initialized)) {
        return false;
    }

    for (attempt = 0u; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t                            observed = noah_runtime_publication_observe(&policy->publication_sequence);
        noah_profile_activation_snapshot_t copied_snapshot;
        uint32_t                           copied_reason_mask;
        uint32_t                           copied_evaluation_count;

        if (noah_runtime_publication_in_flight(observed)) {
            continue;
        }
        copied_snapshot         = policy->last_snapshot;
        copied_reason_mask      = policy->last_reason_mask;
        copied_evaluation_count = policy->evaluation_count;
        if (!noah_runtime_publication_settled(&policy->publication_sequence, observed)) {
            continue;
        }
        if (snapshot) {
            *snapshot = copied_snapshot;
        }
        if (reason_mask) {
            *reason_mask = copied_reason_mask;
        }
        if (evaluation_count) {
            *evaluation_count = copied_evaluation_count;
        }
        return true;
    }
    return false;
}
