// Shared handled-key host-test helpers.
//
// These helpers keep host runtime suites aligned on the explicit
// handled_key_materialize(...) seam without rebuilding the deleted
// position-based compatibility APIs in each test.
#pragma once

#include "users/noah/lib/key/interaction/handled_key.h"

static inline handled_key_materialized_t host_handled_key_materialize_from_authored_resolution(handled_key_resolution_t resolution, handled_key_resolution_ctx_t ctx) {
    (void)ctx;

    handled_key_materialized_t materialized = handled_key_materialized_default(resolution);

    materialized.tap_action            = handled_key_resolution_tap_action(resolution);
    materialized.tap_repeat_count      = handled_key_resolution_tap_repeat_count(resolution);
    materialized.tap_has_more_taps     = resolution.has_more_taps;
    materialized.hold                  = handled_key_resolution_hold(resolution);
    materialized.long_hold             = handled_key_resolution_long_hold(resolution);
    materialized.hold_strategy         = handled_key_resolution_hold_strategy(resolution);
    materialized.tap_resolves_on_press = handled_key_resolution_tap_resolves_on_press(resolution);
    materialized.layer                 = handled_key_resolution_layer(resolution);
    materialized.pd_mode               = handled_key_resolution_pd_mode(resolution);
    materialized.flags                 = resolution.flags;
    materialized.contract              = handled_key_behavior_contract(materialized.hold_strategy, materialized.flags, materialized.tap_action, materialized.pd_mode, materialized.hold, materialized.long_hold);
    return materialized;
}
