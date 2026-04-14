// Shared handled-key host-test helpers.
//
// These helpers keep host runtime suites aligned on the explicit
// handled_key_materialize(...) seam while still letting tests describe
// authored handled-key rows directly.
#pragma once

#include "users/noah/lib/key/interaction/handled_key.h"
#include "users/noah/lib/key/runtime/key_runtime_interaction.h"

static inline handled_key_materialized_t host_handled_key_materialize_from_authored_resolution(handled_key_resolution_t resolution, handled_key_resolution_ctx_t ctx) {
    return handled_key_materialize(resolution, ctx);
}

static inline key_runtime_slot_interaction_t host_key_runtime_slot_interaction_from_authored_resolution(handled_key_resolution_t resolution, handled_key_resolution_ctx_t ctx) {
    return key_runtime_slot_interaction_from_materialized(host_handled_key_materialize_from_authored_resolution(resolution, ctx));
}
