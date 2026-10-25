#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/compat/qmk_combo_origin.h"
#include "users/noah/lib/key/runtime/api.h"
#include "users/noah/lib/macro/macro_payload.h"
#include "users/noah/lib/profile/runtime/profile_activation_policy.h"

#ifdef POINTING_DEVICE_ENABLE
#    include "users/noah/lib/pointing/defs/pointing_mode_flags.h"
static pointing_mode_mask_t   pd_active;
static pointing_mode_source_t pd_source;
bool pointing_mode_local_key_source_active(void) { return pd_active != 0 && pd_source == POINTING_MODE_SOURCE_KEY; }
#endif

static noah_key_runtime_activity_snapshot_t   runtime_activity;
static macro_payload_debug_snapshot_t         macro_activity;
static noah_qmk_combo_origin_debug_snapshot_t combo_activity;
static uint16_t                               managed_usage_count;
static uint8_t                                real_mods;
static uint8_t                                weak_mods;
static uint8_t                                oneshot_mods;
static uint8_t                                locked_oneshot_mods;
static bool                                   oneshot_layer_active;

void noah_key_runtime_activity_snapshot(noah_key_runtime_activity_snapshot_t *out) {
    if (out) {
        *out = runtime_activity;
    }
}

uint16_t owned_keycode_managed_usage_count(void) {
    return managed_usage_count;
}

void macro_payload_debug_snapshot(macro_payload_debug_snapshot_t *out) {
    if (out) {
        *out = macro_activity;
    }
}

void noah_qmk_combo_origin_debug_snapshot(noah_qmk_combo_origin_debug_snapshot_t *out) {
    if (out) {
        *out = combo_activity;
    }
}

uint8_t get_mods(void) {
    return real_mods;
}

uint8_t get_weak_mods(void) {
    return weak_mods;
}

uint8_t get_oneshot_mods(void) {
    return oneshot_mods;
}

uint8_t get_oneshot_locked_mods(void) {
    return locked_oneshot_mods;
}

bool is_oneshot_layer_active(void) {
    return oneshot_layer_active;
}

typedef struct {
    bool    succeeds;
    uint8_t unresolved_count;
    uint8_t calls;
} other_half_fixture_t;

static bool observe_other_half(void *context, uint8_t *unresolved_count) {
    other_half_fixture_t *fixture = context;

    assert(fixture);
    fixture->calls++;
    if (!fixture->succeeds) {
        return false;
    }
    *unresolved_count = fixture->unresolved_count;
    return true;
}

static void reset_activity(void) {
    runtime_activity     = (noah_key_runtime_activity_snapshot_t){0};
    macro_activity       = (macro_payload_debug_snapshot_t){0};
    combo_activity       = (noah_qmk_combo_origin_debug_snapshot_t){0};
    managed_usage_count  = 0u;
    real_mods            = 0u;
    weak_mods            = 0u;
    oneshot_mods         = 0u;
    locked_oneshot_mods  = 0u;
    oneshot_layer_active = false;
}

static void test_reason_bits_are_exact_and_composable(void) {
    noah_profile_activation_snapshot_t snapshot = {0};

    assert(noah_profile_activation_reason_mask(&snapshot) == 0u);
    assert(noah_profile_activation_reason_mask(NULL) == NOAH_PROFILE_ACTIVATION_REASON_INTERNAL);

    snapshot = (noah_profile_activation_snapshot_t){
        .physical_press_count    = 1u,
        .tap_series_count        = 2u,
        .runtime_lease_count     = 3u,
        .deferred_release_count  = 4u,
        .persistent_intent_count = 5u,
        .owned_output_count      = 6u,
        .real_mods               = 1u,
        .oneshot_layer_active    = 1u,
        .macro_engine_state      = MACRO_PAYLOAD_ENGINE_WAITING,
        .combo_active_count      = 1u,
        .unresolved_other_half_count = 1u,
    };
    assert(noah_profile_activation_reason_mask(&snapshot) == (NOAH_PROFILE_ACTIVATION_REASON_PHYSICAL_PRESS | NOAH_PROFILE_ACTIVATION_REASON_TAP_SERIES | NOAH_PROFILE_ACTIVATION_REASON_RUNTIME_LEASE | NOAH_PROFILE_ACTIVATION_REASON_DEFERRED_RELEASE | NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT | NOAH_PROFILE_ACTIVATION_REASON_OWNED_OUTPUT | NOAH_PROFILE_ACTIVATION_REASON_MODIFIER_OR_ONESHOT | NOAH_PROFILE_ACTIVATION_REASON_ONESHOT_LAYER | NOAH_PROFILE_ACTIVATION_REASON_MACRO | NOAH_PROFILE_ACTIVATION_REASON_COMBO | NOAH_PROFILE_ACTIVATION_REASON_SLAVE));
}

static void test_runtime_capture_and_status(void) {
    noah_profile_activation_policy_t   policy;
    noah_profile_activation_snapshot_t snapshot;
    other_half_fixture_t               other_half = {.succeeds = true, .unresolved_count = 0u};
    uint32_t                           reasons;
    uint32_t                           evaluations;

    reset_activity();
    noah_profile_activation_policy_init(&policy, observe_other_half, &other_half);
    assert(noah_profile_activation_policy_safe_boundary(&policy) == 0u);
    assert(other_half.calls == 1u);

    runtime_activity.press_token_count       = 1u;
    runtime_activity.tap_series_count        = 2u;
    runtime_activity.lease_count             = 3u;
    runtime_activity.pending_release_count   = 4u;
    runtime_activity.persistent_intent_count = 5u;
    managed_usage_count                      = 6u;
    weak_mods                                = 0x02u;
    oneshot_mods                             = 0x04u;
    locked_oneshot_mods                      = 0x08u;
    oneshot_layer_active                     = true;
    macro_activity.state                     = MACRO_PAYLOAD_ENGINE_OUTPUT;
    macro_activity.active_hold_count         = 7u;
    combo_activity.pending_count             = 8u;
    combo_activity.active_count              = 9u;
    other_half.unresolved_count              = 10u;

    reasons = noah_profile_activation_policy_safe_boundary(&policy);
    assert((reasons & NOAH_PROFILE_ACTIVATION_REASON_SLAVE) != 0u);
    assert(noah_profile_activation_policy_status(&policy, &snapshot, &reasons, &evaluations));
    assert(snapshot.physical_press_count == 1u);
    assert(snapshot.tap_series_count == 2u);
    assert(snapshot.runtime_lease_count == 3u);
    assert(snapshot.deferred_release_count == 4u);
    assert(snapshot.persistent_intent_count == 5u);
    assert(snapshot.owned_output_count == 6u);
    assert(snapshot.weak_mods == 0x02u);
#ifndef NO_ACTION_ONESHOT
    assert(snapshot.oneshot_mods == 0x04u);
    assert(snapshot.oneshot_locked_mods == 0x08u);
    assert(snapshot.oneshot_layer_active == 1u);
#else
    assert(snapshot.oneshot_mods == 0u);
    assert(snapshot.oneshot_locked_mods == 0u);
    assert(snapshot.oneshot_layer_active == 0u);
#endif
    assert(snapshot.macro_engine_state == MACRO_PAYLOAD_ENGINE_OUTPUT);
    assert(snapshot.macro_hold_count == 7u);
    assert(snapshot.combo_pending_count == 8u);
    assert(snapshot.combo_active_count == 9u);
    assert(snapshot.unresolved_other_half_count == 10u);
    assert(evaluations == 2u);
}

static void test_other_half_observation_fails_closed(void) {
    noah_profile_activation_policy_t policy;
    other_half_fixture_t             other_half = {.succeeds = false, .unresolved_count = 0u};

    reset_activity();
    assert(noah_profile_activation_policy_safe_boundary(NULL) == NOAH_PROFILE_ACTIVATION_REASON_INTERNAL);

    noah_profile_activation_policy_init(&policy, NULL, NULL);
    assert(noah_profile_activation_policy_safe_boundary(&policy) == NOAH_PROFILE_ACTIVATION_REASON_SLAVE);
    assert(policy.last_snapshot.unresolved_other_half_count == 1u);

    noah_profile_activation_policy_init(&policy, observe_other_half, &other_half);
    assert(noah_profile_activation_policy_safe_boundary(&policy) == NOAH_PROFILE_ACTIVATION_REASON_SLAVE);
    assert(other_half.calls == 1u);
    assert(policy.last_snapshot.unresolved_other_half_count == 1u);
}

static void test_evaluation_count_saturates(void) {
    noah_profile_activation_policy_t policy;
    other_half_fixture_t             other_half = {.succeeds = true, .unresolved_count = 0u};

    reset_activity();
    noah_profile_activation_policy_init(&policy, observe_other_half, &other_half);
    policy.evaluation_count = UINT32_MAX;
    assert(noah_profile_activation_policy_safe_boundary(&policy) == 0u);
    assert(policy.evaluation_count == UINT32_MAX);
}

static void test_status_fails_closed_during_publication(void) {
    noah_profile_activation_policy_t   policy;
    noah_profile_activation_snapshot_t snapshot    = {.physical_press_count = 99u};
    other_half_fixture_t               other_half  = {.succeeds = true, .unresolved_count = 0u};
    uint32_t                           reasons     = 99u;
    uint32_t                           evaluations = 99u;

    reset_activity();
    noah_profile_activation_policy_init(&policy, observe_other_half, &other_half);
    policy.publication_sequence = 1u;
    assert(!noah_profile_activation_policy_status(&policy, &snapshot, &reasons, &evaluations));
    assert(snapshot.physical_press_count == 99u);
    assert(reasons == 99u);
    assert(evaluations == 99u);
}

#ifdef POINTING_DEVICE_ENABLE
// A mode its key holds or locks holds activation back; one its layer started
// does not (D-F62): it ends at the boundary instead.
static void test_a_key_started_mode_holds_activation_and_a_layer_started_one_does_not(void) {
    noah_profile_activation_policy_t policy;
    other_half_fixture_t other_half = {.succeeds = true};
    reset_activity();
    noah_profile_activation_policy_init(&policy, observe_other_half, &other_half);
    for (uint8_t i = 0; i < 32; i++) {
        pd_active = (pointing_mode_mask_t)1u << i;
        pd_source = POINTING_MODE_SOURCE_KEY;
        assert(noah_profile_activation_policy_safe_boundary(&policy) == NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT);
        pd_source = POINTING_MODE_SOURCE_LAYER;
        assert(noah_profile_activation_policy_safe_boundary(&policy) == 0);
    }
    pd_source = POINTING_MODE_SOURCE_KEY;
    runtime_activity.persistent_intent_count = UINT8_MAX;
    assert(noah_profile_activation_policy_safe_boundary(&policy) == NOAH_PROFILE_ACTIVATION_REASON_PERSISTENT_INTENT);
    runtime_activity.persistent_intent_count = 0; pd_active = 0;
    assert(noah_profile_activation_policy_safe_boundary(&policy) == 0);
}
#endif

int main(void) {
    test_reason_bits_are_exact_and_composable();
    test_runtime_capture_and_status();
    test_other_half_observation_fails_closed();
    test_evaluation_count_saturates();
    test_status_fails_closed_during_publication();

#ifdef POINTING_DEVICE_ENABLE
    test_a_key_started_mode_holds_activation_and_a_layer_started_one_does_not();
#endif
    puts("profile activation policy host tests passed");
    return 0;
}
