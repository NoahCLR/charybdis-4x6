// ───────────────────────────────────────────────────────────────────────────────
// QMK VIA Live Profile Custom Channel
// ───────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#ifdef VIA_ENABLE

#    include "via.h"

#    include "../profile/protocol/profile_wire_v1.h"
#    include "../profile/storage/profile_storage_layout.h"
#    include "../profile/storage/profile_store_runtime.h"

#    ifndef VIA_FIRMWARE_VERSION
#        define VIA_FIRMWARE_VERSION 0u
#    endif

#    ifdef SPLIT_KEYBOARD
#        define NOAH_PROFILE_SPLIT_CAPABILITY NOAH_PROFILE_FEATURE_SPLIT_KEYBOARD
#    else
#        define NOAH_PROFILE_SPLIT_CAPABILITY 0u
#    endif

#    ifdef RGB_MATRIX_ENABLE
#        define NOAH_PROFILE_LED_COUNT RGB_MATRIX_LED_COUNT
#    else
#        define NOAH_PROFILE_LED_COUNT 0u
#    endif

static noah_profile_wire_v1_read_service_t noah_profile_wire_v1_read_service = {
    .capabilities = {
        .protocol_major                 = 1u,
        .protocol_minor                 = 0u,
        .schema_major                   = 1u,
        .schema_minor                   = 0u,
        // Stage 01 is intentionally read-only. Candidate capacity and domain
        // support remain zero until the corresponding decoders and activation
        // paths exist; the frozen schema ceilings below are declaration
        // metadata, not a claim that writes are accepted.
        .candidate_chunk_max            = 0u,
        .feature_flags                  = NOAH_PROFILE_FEATURE_READ_SURFACE | NOAH_PROFILE_FEATURE_STORAGE_LAYOUT | NOAH_PROFILE_SPLIT_CAPABILITY,
        .action_abi_digest              = 0u,
        .firmware_version               = VIA_FIRMWARE_VERSION,
        .compiled_default_digest        = 0u,
        .compiled_layer_count           = DYNAMIC_KEYMAP_LAYER_COUNT,
        .max_logical_layers             = NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS,
        .max_behavior_rows              = NOAH_PROFILE_WIRE_V1_MAX_BEHAVIOR_ROWS,
        .max_tap_steps_per_behavior     = NOAH_PROFILE_WIRE_V1_MAX_TAP_STEPS_PER_BEHAVIOR,
        .max_populated_behavior_steps   = NOAH_PROFILE_WIRE_V1_MAX_POPULATED_BEHAVIOR_STEPS,
        .max_combos                     = NOAH_PROFILE_WIRE_V1_MAX_COMBOS,
        .max_keys_per_combo             = NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO,
        .max_reusable_rgb_groups        = NOAH_PROFILE_WIRE_V1_MAX_REUSABLE_RGB_GROUPS,
        .max_rgb_stage_group_rows       = NOAH_PROFILE_WIRE_V1_MAX_RGB_STAGE_GROUP_ROWS,
        .physical_led_count             = NOAH_PROFILE_LED_COUNT,
        .led_bitmap_size                = NOAH_PROFILE_WIRE_V1_LED_BITMAP_SIZE,
        .hardcoded_macro_slots          = NOAH_PROFILE_WIRE_V1_MAX_HARDCODED_MACRO_SLOTS,
        .via_macro_slots                = DYNAMIC_KEYMAP_MACRO_COUNT,
        .max_profile_payload            = NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX,
        .profile_slot_payload           = NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX,
        .profile_slot_size              = NOAH_PROFILE_STORAGE_SLOT_A_SIZE,
        .via_macro_bytes                = NOAH_PROFILE_STORAGE_VIA_MACRO_SIZE,
        .supported_domain_mask          = 0u,
    },
    .status = {
        .state_flags = NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT | NOAH_PROFILE_STATE_DIGESTS_UNAVAILABLE,
        .active_kind = NOAH_PROFILE_ACTIVE_COMPILED_ONLY,
    },
};

#    ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
static bool noah_profile_authority_is_conflict(noah_profile_split_authority_state_t state) {
    return state == NOAH_PROFILE_SPLIT_AUTHORITY_CONCURRENT_COMMIT || state == NOAH_PROFILE_SPLIT_AUTHORITY_CORRUPT_SAME_TUPLE;
}

static void noah_profile_channel_refresh_owner_capabilities(const noah_profile_owner_status_t *owner) {
    if (!owner) {
        return;
    }
    noah_profile_wire_v1_read_service.capabilities.feature_flags = NOAH_PROFILE_FEATURE_READ_SURFACE | NOAH_PROFILE_FEATURE_STORAGE_LAYOUT | NOAH_PROFILE_SPLIT_CAPABILITY | NOAH_PROFILE_FEATURE_RGB_SCHEMA | NOAH_PROFILE_FEATURE_KEY_BEHAVIOR_SCHEMA | NOAH_PROFILE_FEATURE_ACTION_ABI_DIGEST | NOAH_PROFILE_FEATURE_COMPILED_PROFILE_HASH;
    noah_profile_wire_v1_read_service.capabilities.action_abi_digest         = owner->action_abi_digest;
    noah_profile_wire_v1_read_service.capabilities.compiled_default_digest   = owner->compiled_default_digest;
    noah_profile_wire_v1_read_service.capabilities.supported_domain_mask     = owner->supported_domain_mask;
    noah_profile_wire_v1_read_service.capabilities.candidate_chunk_max       = 0u;
}

static void noah_profile_channel_latch_owner_status(const noah_profile_owner_status_t *owner) {
    noah_profile_wire_v1_device_status_t *status;

    if (!owner) {
        return;
    }
    status = &noah_profile_wire_v1_read_service.status;
    *status = (noah_profile_wire_v1_device_status_t){
        .source_digest                 = owner->compiled_default_digest,
        .compiled_default_digest       = owner->compiled_default_digest,
        .candidate_transaction_id      = owner->candidate.transaction_id,
        .last_committed_transaction_id = owner->last_committed_transaction_id,
        .validation_state              = (uint8_t)owner->candidate.state,
        .last_error                    = (uint8_t)owner->candidate.error.code,
    };

    if (owner->provider_known) {
        status->active_digest     = owner->active.payload_digest;
        status->active_generation = owner->active.generation;
        if (owner->active.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE) {
            status->active_kind        = NOAH_PROFILE_ACTIVE_COMMITTED;
            status->active_origin_half = owner->active.origin;
        } else {
            status->state_flags |= NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT;
            status->active_kind = NOAH_PROFILE_ACTIVE_COMPILED_ONLY;
        }
    } else {
        status->state_flags |= NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT;
        status->active_digest = owner->compiled_default_digest;
        status->active_kind   = NOAH_PROFILE_ACTIVE_COMPILED_ONLY;
    }
    if (owner->has_pending) {
        status->pending_digest = owner->pending.payload_digest;
    }
    if (owner->has_committed) {
        status->state_flags |= NOAH_PROFILE_STATE_COMMITTED_VALID;
        status->committed_digest      = owner->committed.payload_digest;
        status->committed_generation  = owner->committed.generation;
        status->committed_origin_half = owner->committed.origin_half;
    }
    if (owner->candidate_pending || owner->has_pending) {
        status->state_flags |= NOAH_PROFILE_STATE_CANDIDATE_PENDING;
    }
    if (owner->has_pending && owner->safe_boundary_reason_mask != 0u) {
        status->state_flags |= NOAH_PROFILE_STATE_WAITING_SAFE_BOUNDARY;
    }
    if (owner->peer_known) {
        status->state_flags |= NOAH_PROFILE_STATE_PEER_KNOWN;
        if (owner->peer.has_profile) {
            status->peer_generation  = owner->peer.generation;
            status->peer_origin_half = owner->peer.origin_half;
        }
    }
    if (owner->peer_converged) {
        status->state_flags |= NOAH_PROFILE_STATE_PEER_CONVERGED;
    }
    if (owner->owner_state == NOAH_PROFILE_OWNER_GENERATION_CONFLICT || noah_profile_authority_is_conflict(owner->authority_state)) {
        status->conflict_count = 1u;
    }
}

static bool noah_profile_channel_refresh_owner(bool latch_status) {
    noah_profile_owner_status_t owner;

    if (!noah_profile_store_runtime_owner_status(&owner)) {
        return false;
    }
    noah_profile_channel_refresh_owner_capabilities(&owner);
    if (latch_status) {
        noah_profile_channel_latch_owner_status(&owner);
    }
    return true;
}
#    endif

static void noah_profile_channel_refresh_store_status(void) {
    const noah_profile_store_record_t *committed = noah_profile_store_runtime_committed();

    noah_profile_wire_v1_read_service.status = (noah_profile_wire_v1_device_status_t){
        .state_flags = NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT | NOAH_PROFILE_STATE_DIGESTS_UNAVAILABLE,
        .active_kind = NOAH_PROFILE_ACTIVE_COMPILED_ONLY,
    };

    if (committed) {
        noah_profile_wire_v1_read_service.status.state_flags |= NOAH_PROFILE_STATE_COMMITTED_VALID;
        noah_profile_wire_v1_read_service.status.committed_digest      = committed->payload_digest;
        noah_profile_wire_v1_read_service.status.committed_generation  = committed->generation;
        noah_profile_wire_v1_read_service.status.committed_origin_half = committed->origin_half;
    } else if (noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_GENERATION_CONFLICT) {
        noah_profile_wire_v1_read_service.status.conflict_count = 1u;
    }
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    if (data && length == NOAH_PROFILE_WIRE_V1_REPORT_SIZE && data[0] == NOAH_PROFILE_WIRE_V1_COMMAND_GET && data[1] == NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL && data[2] == NOAH_PROFILE_WIRE_V1_VALUE_STATUS && data[4] == 0u) {
#    ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
        if (!noah_profile_channel_refresh_owner(true)) {
            noah_profile_channel_refresh_store_status();
        }
#    else
        noah_profile_channel_refresh_store_status();
#    endif
    }
#    ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
    if (data && length == NOAH_PROFILE_WIRE_V1_REPORT_SIZE && data[0] == NOAH_PROFILE_WIRE_V1_COMMAND_GET && data[1] == NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL && data[2] == NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY) {
        (void)noah_profile_channel_refresh_owner(false);
    }
#    endif
    if (noah_profile_wire_v1_handle_get(&noah_profile_wire_v1_read_service, data, length)) {
        return;
    }
    if (data && length > 0u) {
        data[0] = id_unhandled;
    }
}

_Static_assert(DYNAMIC_KEYMAP_LAYER_COUNT <= UINT8_MAX, "compiled layer count must fit the capability frame");
_Static_assert(DYNAMIC_KEYMAP_LAYER_COUNT <= NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS, "compiled layer count must fit the declared Profile Wire ceiling");
_Static_assert(DYNAMIC_KEYMAP_MACRO_COUNT <= UINT8_MAX, "VIA macro count must fit the capability frame");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX <= UINT16_MAX, "profile payload capacity must fit the capability frame");
_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_SIZE <= UINT16_MAX, "profile slot size must fit the capability frame");
_Static_assert(NOAH_PROFILE_STORAGE_VIA_MACRO_SIZE <= UINT16_MAX, "VIA macro capacity must fit the capability frame");
_Static_assert(VIA_FIRMWARE_VERSION != 0u, "Profile Wire firmware must advertise a meaningful VIA firmware version");
_Static_assert((uint8_t)NOAH_PROFILE_WIRE_V1_COMMAND_GET == (uint8_t)id_custom_get_value, "Profile Wire custom-get routing drifted from QMK VIA");
_Static_assert((uint8_t)NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL == (uint8_t)id_custom_channel, "Profile Wire custom channel drifted from QMK VIA");
_Static_assert(NOAH_PROFILE_WIRE_V1_REPORT_SIZE == 32u, "Profile Wire requires the reviewed 32-byte QMK Raw HID report");

#    ifdef RGB_MATRIX_ENABLE
_Static_assert(NOAH_PROFILE_LED_COUNT == NOAH_PROFILE_WIRE_V1_MAX_PHYSICAL_LEDS, "compiled RGB geometry drifted from the declared Profile Wire ceiling");
#    endif

#    undef NOAH_PROFILE_LED_COUNT
#    undef NOAH_PROFILE_SPLIT_CAPABILITY

#endif
