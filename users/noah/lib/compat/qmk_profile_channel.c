// ───────────────────────────────────────────────────────────────────────────────
// QMK Raw HID Profile Wire Channel
// ───────────────────────────────────────────────────────────────────────────────
//
// The firmware answers Raw HID itself (D-F31): every report reaches
// raw_hid_receive below, goes to the Profile Wire handlers, and is sent back.
// A report none of them takes is answered with the unhandled command byte.

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <string.h>

#include "qmk_split_diagnostics.h"
#include "../profile/runtime/active_profile_pd_runtime.h"

#if defined(NOAH_LIVE_PROFILE_MUTATION_ENABLE) && !defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
#    error "live-profile mutation requires the live-profile owner"
#endif

#ifdef NOAH_STACK_BUDGET_ENABLE
#    define NOAH_PROFILE_CHANNEL_STACK_BOUNDARY __attribute__((noinline))
#else
#    define NOAH_PROFILE_CHANNEL_STACK_BOUNDARY
#endif

#ifdef RAW_ENABLE

#    include "raw_hid.h"
#    include "usb_descriptor.h"
#    include "qmk_combo_readback.h"
#    include "qmk_profile_edit_channel.h"
#    include "qmk_profile_flash_hooks.h"
#    include "qmk_portable_profile.h"
#    include "qmk_usb_product_name.h"

#    include "../profile/protocol/profile_wire_v1.h"
#    include "../profile/storage/profile_storage_layout.h"
#    include "../profile/storage/profile_store_runtime.h"
#    include "../state/diagnostics/runtime_diag.h"

#    include "qmk_profile_capabilities.h"

static noah_profile_wire_v1_read_service_t noah_profile_wire_v1_read_service = {
    .capabilities = NOAH_PROFILE_CHANNEL_BASE_CAPABILITIES,
    .status =
        {
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
    noah_profile_channel_apply_owner_capabilities(&noah_profile_wire_v1_read_service.capabilities, owner->action_abi_digest, owner->compiled_default_digest, owner->supported_domain_mask);
}

static void noah_profile_channel_latch_owner_status(const noah_profile_owner_status_t *owner) {
    noah_profile_wire_v1_device_status_t *status;
    bool                                  booting;

    if (!owner) {
        return;
    }
    status  = &noah_profile_wire_v1_read_service.status;
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
        if (owner->active.kind == NOAH_ACTIVE_PROFILE_KIND_VALIDATED_PROFILE) {
            status->active_kind        = NOAH_PROFILE_ACTIVE_SAVED;
            status->active_origin_half = owner->active.origin;
        } else if (owner->active.kind == NOAH_ACTIVE_PROFILE_KIND_WORKING_COPY) {
            status->active_kind        = NOAH_PROFILE_ACTIVE_WORKING;
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
    // Pending while the boot has not decided which profile runs (D-F61): its
    // compiled defaults run meanwhile, and nothing has failed. After the boot,
    // unpublished pointing slots are an activation that failed.
    booting = noah_profile_owner_booting(owner->owner_state);
    if (booting || !noah_active_profile_pd_ready()) {
        status->state_flags |= NOAH_PROFILE_STATE_DIGESTS_UNAVAILABLE;
        status->state_flags &= (uint16_t)~NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT;
        status->active_digest = 0;
        status->active_generation = 0;
        status->active_kind = NOAH_PROFILE_ACTIVE_PENDING;
        if (!booting) {
            status->last_error = NOAH_PROFILE_CANDIDATE_V1_ERROR_ACTIVATION_FAILED;
        }
    }
    if (owner->has_pending) {
        status->pending_digest = owner->pending.payload_digest;
    }
    if (owner->has_saved) {
        status->state_flags |= NOAH_PROFILE_STATE_SAVED_VALID;
        status->saved_digest          = owner->saved.payload_digest;
        status->saved_generation      = owner->saved.generation;
        status->saved_origin_half = owner->saved.origin_half;
    }
    if (owner->candidate_pending || owner->has_pending) {
        status->state_flags |= NOAH_PROFILE_STATE_CANDIDATE_PENDING;
    }
    if (owner->has_pending && owner->safe_boundary_reason_mask != 0u) {
        status->state_flags |= NOAH_PROFILE_STATE_WAITING_SAFE_BOUNDARY;
    }
    if (owner->other_half_known) {
        status->state_flags |= NOAH_PROFILE_STATE_PEER_KNOWN;
        if (owner->other_half.has_profile) {
            status->slave_generation = owner->other_half.generation;
            status->slave_origin_half = owner->other_half.origin_half;
        }
    }
    if (owner->saved_profiles_agree) {
        status->state_flags |= NOAH_PROFILE_STATE_PEER_CONVERGED;
    }
    if (owner->peer_cleanup_pending) {
        status->state_flags |= NOAH_PROFILE_STATE_PEER_CLEANUP_PENDING;
    }
    if (owner->stored_earlier_format) {
        status->state_flags |= NOAH_PROFILE_STATE_STORED_EARLIER_FORMAT;
    }
    if (owner->owner_state == NOAH_PROFILE_OWNER_GENERATION_CONFLICT || owner->owner_state == NOAH_PROFILE_OWNER_CONCURRENT_COMMIT || noah_profile_authority_is_conflict(owner->authority_state)) {
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
    const noah_profile_store_record_t *saved = noah_profile_store_runtime_saved();

    noah_profile_wire_v1_read_service.status = (noah_profile_wire_v1_device_status_t){
        .state_flags = NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT | NOAH_PROFILE_STATE_DIGESTS_UNAVAILABLE,
        .active_kind = NOAH_PROFILE_ACTIVE_COMPILED_ONLY,
    };

    if (saved) {
        noah_profile_wire_v1_read_service.status.state_flags |= NOAH_PROFILE_STATE_SAVED_VALID;
        noah_profile_wire_v1_read_service.status.saved_digest          = saved->payload_digest;
        noah_profile_wire_v1_read_service.status.saved_generation      = saved->generation;
        noah_profile_wire_v1_read_service.status.saved_origin_half = saved->origin_half;
    } else if (noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_GENERATION_CONFLICT) {
        noah_profile_wire_v1_read_service.status.conflict_count = 1u;
    }
}

#    ifdef NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE
static bool noah_profile_channel_handle_cadence_get(uint8_t *data, uint8_t length) {
    if (!data || length != NOAH_PROFILE_WIRE_V1_REPORT_SIZE || data[0] != NOAH_PROFILE_WIRE_V1_COMMAND_GET || data[1] != NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL || data[2] != NOAH_RUNTIME_CADENCE_WIRE_VALUE) {
        return false;
    }
    for (uint8_t index = 5u; index < NOAH_PROFILE_WIRE_V1_REPORT_SIZE; index++) {
        if (data[index] != 0u) {
            memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
            data[5] = NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED;
            return true;
        }
    }
    if (data[3] == 0u || data[4] >= NOAH_RUNTIME_CADENCE_WIRE_PAGES) {
        memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
        data[5] = data[3] == 0u ? NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED : NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE;
        return true;
    }
    data[5] = NOAH_PROFILE_WIRE_V1_STATUS_OK;
    data[6] = NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE;
    if (!noah_runtime_cadence_wire_page(data[4], &data[7])) {
        memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
        data[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
    }
    return true;
}
#    endif

static void noah_profile_channel_write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)(value & 0xffu);
    target[1] = (uint8_t)((value >> 8) & 0xffu);
    target[2] = (uint8_t)((value >> 16) & 0xffu);
    target[3] = (uint8_t)((value >> 24) & 0xffu);
}

// Saved-payload readback.
//
// Page 0 reports the saved generation, digest and length; pages 1..N carry
// raw payload bytes. Coherence is the host's job and is cheap: generation only
// ever increases, so re-reading page 0 after the chunks proves nothing was
// saved in between. That keeps a full report of payload in every chunk
// instead of spending four bytes per chunk on a sequence number.
//
// This is the read D-026 requires. The READ_SURFACE capability bit describes
// status reporting and is not this.
static bool noah_profile_channel_handle_payload_get(uint8_t *data, uint8_t length) {
    const noah_profile_store_record_t  *record = NULL;
    noah_profile_compiled_v1_metadata_t compiled_metadata;
    uint32_t                            payload_length;
    uint32_t                            offset;
    uint32_t                            remaining;
    uint16_t                            page;
    uint8_t                             chunk;
    bool                                compiled;

    if (!data || length != NOAH_PROFILE_WIRE_V1_REPORT_SIZE || data[0] != NOAH_PROFILE_WIRE_V1_COMMAND_GET || data[1] != NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL) {
        return false;
    }
    if (data[2] != NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD && data[2] != NOAH_PROFILE_WIRE_V1_VALUE_COMPILED) {
        return false;
    }
    compiled = data[2] == NOAH_PROFILE_WIRE_V1_VALUE_COMPILED;
    page     = noah_profile_wire_v1_wide_page(data);
    for (uint8_t index = NOAH_PROFILE_WIRE_V1_WIDE_REQUEST_FIXED; index < NOAH_PROFILE_WIRE_V1_REPORT_SIZE; index++) {
        if (data[index] != 0u) {
            memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
            data[5] = NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED;
            return true;
        }
    }
    if (data[3] == 0u) {
        memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
        data[5] = NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED;
        return true;
    }

    if (compiled) {
        if (!noah_profile_store_runtime_compiled_metadata(&compiled_metadata)) {
            memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
            data[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
            return true;
        }
        payload_length = compiled_metadata.byte_length;
    } else {
        record = noah_profile_store_runtime_saved();
        if (!record) {
            memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
            data[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
            return true;
        }
        payload_length = record->payload_length;
    }

    memset(&data[6], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 6u);
    if (page == NOAH_PROFILE_WIRE_V1_PAYLOAD_METADATA_PAGE) {
        data[5] = NOAH_PROFILE_WIRE_V1_STATUS_OK;
        data[6] = NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE;
        data[7] = 2u; // layout version: a 32-bit payload length (D-F28)
        data[8] = NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE;
        noah_profile_channel_write_u32(&data[9], payload_length);
        if (compiled) {
            // Compiled defaults have no generation: they are what the firmware
            // was built with, not something saved.
            noah_profile_channel_write_u32(&data[13], 0u);
            noah_profile_channel_write_u32(&data[17], compiled_metadata.digest);
            noah_profile_channel_write_u32(&data[21], compiled_metadata.crc32);
            data[25] = NOAH_PROFILE_STORE_SCHEMA_MAJOR;
            data[26] = NOAH_PROFILE_STORE_SCHEMA_MINOR;
            data[27] = compiled_metadata.domain_mask;
        } else {
            noah_profile_channel_write_u32(&data[13], record->generation);
            noah_profile_channel_write_u32(&data[17], record->payload_digest);
            noah_profile_channel_write_u32(&data[21], record->payload_crc32);
            data[25] = record->schema_major;
            data[26] = record->schema_minor;
            data[27] = record->domain_mask;
            data[28] = record->origin_half;
            data[29] = record->flags;
        }
        return true;
    }

    offset = (uint32_t)(page - 1u) * NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE;
    if (offset >= payload_length) {
        memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
        data[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE;
        return true;
    }
    remaining = payload_length - offset;
    chunk     = remaining < NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE ? (uint8_t)remaining : NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE;

    if (!(compiled ? noah_profile_store_runtime_read_compiled(offset, &data[7], chunk) : noah_profile_store_runtime_read_saved(offset, &data[7], chunk))) {
        memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
        data[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
        return true;
    }
    data[5] = NOAH_PROFILE_WIRE_V1_STATUS_OK;
    data[6] = chunk;
    return true;
}

NOAH_PROFILE_CHANNEL_STACK_BOUNDARY static void noah_profile_channel_dispatch(uint8_t *data, uint8_t length) {
    if (noah_split_diagnostics_command(data, length)) return;
    if (noah_qmk_combo_readback_get(data, length)) return;
    if (noah_qmk_profile_flash_wire_get(data, length)) return;
    if (noah_qmk_profile_edit_channel(data, length)) return;
#    ifdef NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE
    if (noah_profile_channel_handle_cadence_get(data, length)) {
        return;
    }
#    endif
#    ifdef NOAH_LIVE_PROFILE_MUTATION_ENABLE
    if (data && length == NOAH_PROFILE_WIRE_V1_REPORT_SIZE && data[0] == NOAH_PROFILE_WIRE_V1_COMMAND_GET && data[1] == NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL && data[2] == NOAH_PROFILE_CANDIDATE_V1_VALUE_STATUS) {
        noah_profile_candidate_v1_status_t      candidate;
        noah_profile_candidate_v1_copy_status_t other_half;

        if ((data[4] == 1u || data[4] == 2u) && noah_profile_store_runtime_other_half_transfer(&other_half) && noah_profile_candidate_v1_handle_copy_status_get(&other_half, data, length)) {
            return;
        }
        if (noah_profile_store_runtime_candidate_status(&candidate) && noah_profile_candidate_v1_handle_status_get(&candidate, data, length)) {
            return;
        }
    }
    if (noah_profile_store_runtime_candidate_receive(data, length)) {
        return;
    }
#    endif
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
    if (noah_qmk_portable_profile_get(data, length)) return;
    if (noah_profile_channel_handle_payload_get(data, length)) {
        return;
    }
    if (noah_profile_wire_v1_handle_get(&noah_profile_wire_v1_read_service, data, length)) {
        return;
    }
    data[0] = NOAH_PROFILE_WIRE_V1_COMMAND_UNHANDLED;
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    if (!data || length == 0u) {
        return;
    }
    // A USB rename waits for the host to go quiet.
    noah_qmk_usb_product_name_note_host_frame();
    noah_profile_channel_dispatch(data, length);
    raw_hid_send(data, length);
}

_Static_assert(LAYER_COUNT <= UINT8_MAX, "compiled layer count must fit the capability frame");
_Static_assert(LAYER_COUNT <= NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS, "compiled layer count must fit the declared Profile Wire ceiling");
_Static_assert(NOAH_PROFILE_MACROS_V1_SLOTS <= UINT8_MAX, "macro count must fit the capability frame");
_Static_assert(NOAH_FIRMWARE_VERSION != 0u, "Profile Wire firmware must advertise a meaningful firmware version");
_Static_assert(NOAH_PROFILE_WIRE_V1_REPORT_SIZE == 32u, "Profile Wire requires the reviewed 32-byte QMK Raw HID report");
_Static_assert(RAW_EPSIZE == NOAH_PROFILE_WIRE_V1_REPORT_SIZE, "QMK's Raw HID report is not the Profile Wire report size");

#    ifdef RGB_MATRIX_ENABLE
_Static_assert(NOAH_PROFILE_LED_COUNT == NOAH_PROFILE_WIRE_V1_MAX_PHYSICAL_LEDS, "compiled RGB geometry drifted from the declared Profile Wire ceiling");
#    endif

#    undef NOAH_PROFILE_LED_COUNT
#    undef NOAH_PROFILE_MUTATION_CHUNK_MAX
#    undef NOAH_PROFILE_MUTATION_CAPABILITIES
#    undef NOAH_PROFILE_SPLIT_CAPABILITY
#    undef NOAH_PROFILE_POINTING_CAPABILITY

#endif

#undef NOAH_PROFILE_CHANNEL_STACK_BOUNDARY
