#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "via.h"

#include "users/noah/lib/profile/protocol/profile_wire_v1.h"
#include "users/noah/lib/profile/storage/profile_storage_layout.h"
#include "users/noah/lib/profile/storage/profile_store_runtime.h"
#include "users/noah/lib/state/diagnostics/runtime_diag.h"
#include "users/noah/lib/compat/qmk_via_split_sync.h"

void via_custom_value_command_kb(uint8_t *data, uint8_t length);

enum {
    WIRE_PAYLOAD = 7u,
};

static noah_profile_store_runtime_state_t store_runtime_state;
static noah_profile_store_record_t        store_runtime_record;
static bool                               store_runtime_has_committed;

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)
uint16_t noah_qmk_via_storage_region_size(noah_qmk_via_sync_region_t region) {
    switch (region) {
        case NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG:
            return 2u;
        case NOAH_QMK_VIA_SYNC_REGION_KEYMAP:
            return 960u;
        case NOAH_QMK_VIA_SYNC_REGION_MACRO:
            return 7191u;
        default:
            return 0u;
    }
}

bool noah_qmk_via_logical_submit(uint16_t transaction_id, const noah_qmk_via_sync_frame_t *request) {
    (void)transaction_id;
    (void)request;
    return true;
}

bool noah_qmk_via_logical_status(noah_qmk_via_logical_status_t *status) {
    if (!status) return false;
    *status = (noah_qmk_via_logical_status_t){0};
    return true;
}
#endif

#ifdef NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE
bool noah_runtime_cadence_wire_page(uint8_t page, uint8_t payload[NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE]) {
    if (!payload || page >= NOAH_RUNTIME_CADENCE_WIRE_PAGES) return false;
    memset(payload, 0, NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE);
    payload[0] = 1u;
    payload[1] = page;
    return true;
}
#endif

#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
static noah_profile_owner_status_t live_owner_status;

#    ifdef NOAH_LIVE_PROFILE_MUTATION_ENABLE
static uint8_t mutation_receive_count;

bool noah_profile_store_runtime_candidate_status(noah_profile_candidate_v1_status_t *status) {
    if (!status) return false;
    *status = live_owner_status.candidate;
    return true;
}

bool noah_profile_store_runtime_candidate_receive(uint8_t *frame, size_t length) {
    if (!frame || length != NOAH_PROFILE_WIRE_V1_REPORT_SIZE || frame[0] != NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET || frame[1] != NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL || frame[2] != NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN) {
        return false;
    }
    mutation_receive_count++;
    noah_profile_candidate_v1_encode_ack(frame, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8);
    return true;
}
#    endif

bool noah_profile_store_runtime_owner_status(noah_profile_owner_status_t *status) {
    if (!status) return false;
    *status = live_owner_status;
    return true;
}

static void initialize_live_owner_status(void) {
    memset(&live_owner_status, 0, sizeof(live_owner_status));
    live_owner_status.owner_state             = NOAH_PROFILE_OWNER_READY_VALIDATED;
    live_owner_status.compiled_default_digest = UINT32_C(0x10203040);
    live_owner_status.action_abi_digest       = UINT32_C(0x50607080);
    live_owner_status.supported_domain_mask   = NOAH_PROFILE_CANDIDATE_V1_KNOWN_DOMAINS;
    live_owner_status.provider_known          = true;
    live_owner_status.active                  = (noah_effective_profile_identity_t){
        .generation     = 7u,
        .payload_digest = UINT32_C(0x11121314),
        .origin         = 1u,
        .kind           = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE,
    };
    live_owner_status.pending = (noah_effective_profile_identity_t){
        .generation     = 8u,
        .payload_digest = UINT32_C(0x21222324),
        .origin         = 0u,
        .kind           = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE,
    };
    live_owner_status.has_pending               = true;
    live_owner_status.safe_boundary_reason_mask = 1u;
    live_owner_status.committed                 = (noah_profile_split_descriptor_t){
        .generation     = 7u,
        .payload_digest = UINT32_C(0x31323334),
        .origin_half    = 1u,
        .readable       = true,
        .has_profile    = true,
    };
    live_owner_status.has_committed = true;
    live_owner_status.candidate     = (noah_profile_candidate_v1_status_t){
        .state          = NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED,
        .transaction_id = UINT16_C(0x4142),
    };
    live_owner_status.candidate_pending             = true;
    live_owner_status.last_committed_transaction_id = UINT16_C(0x5152);
    live_owner_status.peer                          = (noah_profile_split_descriptor_t){
        .generation  = 9u,
        .origin_half = 0u,
        .readable    = true,
        .has_profile = true,
    };
    live_owner_status.peer_known      = true;
    live_owner_status.authority_state = NOAH_PROFILE_SPLIT_AUTHORITY_PEER_NEWER;
}
#endif

noah_profile_store_runtime_state_t noah_profile_store_runtime_state(void) {
    return store_runtime_state;
}

const noah_profile_store_record_t *noah_profile_store_runtime_committed(void) {
    return store_runtime_has_committed ? &store_runtime_record : NULL;
}

// Stands in for EEPROM. Each byte is derived from its offset so a chunk served
// from the wrong address is visible in the assertion rather than plausible.
static uint8_t store_payload_byte(uint16_t offset) {
    return (uint8_t)(offset * 7u + 3u);
}

static bool store_read_committed_fails;
static bool store_has_compiled = true;

// The compiled defaults are a virtual view over authored const data, so the
// stub derives them the same way: byte value from offset, distinct from the
// committed pattern so a source mix-up is visible.
static uint8_t store_compiled_byte(uint16_t offset) {
    return (uint8_t)(offset * 11u + 5u);
}

bool noah_profile_store_runtime_compiled_metadata(noah_profile_compiled_v1_metadata_t *metadata) {
    if (!store_has_compiled || !metadata) {
        return false;
    }
    memset(metadata, 0, sizeof(*metadata));
    metadata->byte_length = 40u;
    metadata->digest      = UINT32_C(0x01020304);
    metadata->crc32       = UINT32_C(0x05060708);
    metadata->domain_mask = 0x30u;
    return true;
}

bool noah_profile_store_runtime_read_compiled(uint16_t offset, uint8_t *target, uint16_t length) {
    if (!store_has_compiled || !target || length == 0u || (uint32_t)offset + (uint32_t)length > 40u) {
        return false;
    }
    for (uint16_t index = 0u; index < length; index++) {
        target[index] = store_compiled_byte((uint16_t)(offset + index));
    }
    return true;
}

bool noah_profile_store_runtime_read_committed(uint16_t offset, uint8_t *target, uint16_t length) {
    if (store_read_committed_fails || !store_runtime_has_committed || !target || length == 0u) {
        return false;
    }
    if ((uint32_t)offset + (uint32_t)length > (uint32_t)store_runtime_record.payload_length) {
        return false;
    }
    for (uint16_t index = 0u; index < length; index++) {
        target[index] = store_payload_byte((uint16_t)(offset + index));
    }
    return true;
}

static uint16_t read_u16(const uint8_t *source) {
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8u);
}

static uint32_t read_u32(const uint8_t *source) {
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8u) | ((uint32_t)source[2] << 16u) | ((uint32_t)source[3] << 24u);
}

static void make_request(uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], uint8_t value, uint8_t request_id, uint8_t page) {
    memset(frame, 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE);
    frame[0] = id_custom_get_value;
    frame[1] = id_custom_channel;
    frame[2] = value;
    frame[3] = request_id;
    frame[4] = page;
}

static uint32_t expected_feature_flags(void) {
    uint32_t flags = NOAH_PROFILE_FEATURE_READ_SURFACE | NOAH_PROFILE_FEATURE_STORAGE_LAYOUT;
#ifdef SPLIT_KEYBOARD
    flags |= NOAH_PROFILE_FEATURE_SPLIT_KEYBOARD;
#endif
#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
    flags |= NOAH_PROFILE_FEATURE_RGB_SCHEMA | NOAH_PROFILE_FEATURE_KEY_BEHAVIOR_SCHEMA | NOAH_PROFILE_FEATURE_ACTION_ABI_DIGEST | NOAH_PROFILE_FEATURE_COMPILED_PROFILE_HASH;
#endif
#ifdef NOAH_LIVE_PROFILE_MUTATION_ENABLE
    flags |= NOAH_PROFILE_FEATURE_CANDIDATE_WRITE | NOAH_PROFILE_FEATURE_PERSISTENT_COMMIT | NOAH_PROFILE_FEATURE_RUNTIME_ACTIVATION | NOAH_PROFILE_FEATURE_PEER_RECONCILIATION | NOAH_PROFILE_FEATURE_ATOMIC_LOGICAL_APPLY;
#endif
    return flags;
}

static void test_capability_hook(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];
    uint8_t expected_candidate_chunk = 0u;

#ifdef NOAH_LIVE_PROFILE_MUTATION_ENABLE
    expected_candidate_chunk = NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX;
#endif

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0x31u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[0] == id_custom_get_value);
    assert(frame[1] == id_custom_channel);
    assert(frame[2] == NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY);
    assert(frame[3] == 0x31u);
    assert(frame[4] == 0u);
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[6] == NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE);
    assert(frame[WIRE_PAYLOAD + 6u] == NOAH_PROFILE_WIRE_V1_REPORT_SIZE);
    assert(frame[WIRE_PAYLOAD + 7u] == expected_candidate_chunk);
    assert(read_u32(&frame[WIRE_PAYLOAD + 9u]) == expected_feature_flags());
#ifndef NOAH_LIVE_PROFILE_MUTATION_ENABLE
    assert((read_u32(&frame[WIRE_PAYLOAD + 9u]) & (NOAH_PROFILE_FEATURE_CANDIDATE_WRITE | NOAH_PROFILE_FEATURE_PERSISTENT_COMMIT | NOAH_PROFILE_FEATURE_RUNTIME_ACTIVATION | NOAH_PROFILE_FEATURE_PEER_RECONCILIATION)) == 0u);
#else
    assert((read_u32(&frame[WIRE_PAYLOAD + 9u]) & (NOAH_PROFILE_FEATURE_CANDIDATE_WRITE | NOAH_PROFILE_FEATURE_PERSISTENT_COMMIT | NOAH_PROFILE_FEATURE_RUNTIME_ACTIVATION | NOAH_PROFILE_FEATURE_PEER_RECONCILIATION)) == (NOAH_PROFILE_FEATURE_CANDIDATE_WRITE | NOAH_PROFILE_FEATURE_PERSISTENT_COMMIT | NOAH_PROFILE_FEATURE_RUNTIME_ACTIVATION | NOAH_PROFILE_FEATURE_PEER_RECONCILIATION));
#endif
#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
    assert((read_u32(&frame[WIRE_PAYLOAD + 9u]) & (NOAH_PROFILE_FEATURE_RGB_SCHEMA | NOAH_PROFILE_FEATURE_KEY_BEHAVIOR_SCHEMA)) == (NOAH_PROFILE_FEATURE_RGB_SCHEMA | NOAH_PROFILE_FEATURE_KEY_BEHAVIOR_SCHEMA));
    assert(read_u32(&frame[WIRE_PAYLOAD + 13u]) == live_owner_status.action_abi_digest);
    assert(read_u32(&frame[WIRE_PAYLOAD + 21u]) == live_owner_status.compiled_default_digest);
#else
    assert((read_u32(&frame[WIRE_PAYLOAD + 9u]) & (NOAH_PROFILE_FEATURE_RGB_SCHEMA | NOAH_PROFILE_FEATURE_KEY_BEHAVIOR_SCHEMA)) == 0u);
#endif
    assert(read_u32(&frame[WIRE_PAYLOAD + 17u]) == VIA_FIRMWARE_VERSION);

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0x32u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[WIRE_PAYLOAD] == DYNAMIC_KEYMAP_LAYER_COUNT);
#ifdef RGB_MATRIX_ENABLE
    assert(frame[WIRE_PAYLOAD + 9u] == RGB_MATRIX_LED_COUNT);
#else
    assert(frame[WIRE_PAYLOAD + 9u] == 0u);
#endif
#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
    assert(frame[WIRE_PAYLOAD + 21u] == NOAH_PROFILE_CANDIDATE_V1_KNOWN_DOMAINS);
#else
    assert(frame[WIRE_PAYLOAD + 21u] == 0u);
#endif
}

#ifndef NOAH_LIVE_PROFILE_OWNER_ENABLE
static void test_status_surfaces_read_only_store_discovery(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    store_runtime_state         = NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING;
    store_runtime_has_committed = false;
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x41u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(read_u16(&frame[WIRE_PAYLOAD + 2u]) == (NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT | NOAH_PROFILE_STATE_DIGESTS_UNAVAILABLE));
    assert(read_u32(&frame[WIRE_PAYLOAD + 20u]) == 0u);
    assert(frame[WIRE_PAYLOAD + 24u] == NOAH_PROFILE_ACTIVE_COMPILED_ONLY);

    store_runtime_state         = NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND;
    store_runtime_has_committed = true;
    store_runtime_record        = (noah_profile_store_record_t){
        .payload_digest = UINT32_C(0x78563412),
        .generation     = UINT32_C(0x01020304),
        .origin_half    = 1u,
    };
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x42u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(read_u16(&frame[WIRE_PAYLOAD + 2u]) == (NOAH_PROFILE_STATE_ACTIVE_IS_COMPILED_DEFAULT | NOAH_PROFILE_STATE_COMMITTED_VALID | NOAH_PROFILE_STATE_DIGESTS_UNAVAILABLE));
    assert(read_u32(&frame[WIRE_PAYLOAD + 20u]) == UINT32_C(0x78563412));
    assert(frame[WIRE_PAYLOAD + 24u] == NOAH_PROFILE_ACTIVE_COMPILED_ONLY);

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x43u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(read_u32(&frame[WIRE_PAYLOAD + 5u]) == UINT32_C(0x01020304));
    assert(frame[WIRE_PAYLOAD + 9u] == 1u);

    store_runtime_state         = NOAH_PROFILE_STORE_RUNTIME_GENERATION_CONFLICT;
    store_runtime_has_committed = false;
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x44u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    // Page one remains paired with the last page-zero observation.
    assert(read_u16(&frame[WIRE_PAYLOAD + 19u]) == 0u);
    assert(read_u32(&frame[WIRE_PAYLOAD + 5u]) == UINT32_C(0x01020304));
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x45u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x46u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(read_u16(&frame[WIRE_PAYLOAD + 19u]) == 1u);
    assert(read_u32(&frame[WIRE_PAYLOAD + 5u]) == 0u);
}
#else
static void test_status_surfaces_coherent_owner_snapshot(void) {
    uint8_t  frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];
    uint16_t expected_flags = NOAH_PROFILE_STATE_COMMITTED_VALID | NOAH_PROFILE_STATE_CANDIDATE_PENDING | NOAH_PROFILE_STATE_PEER_KNOWN | NOAH_PROFILE_STATE_WAITING_SAFE_BOUNDARY;

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x41u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(read_u16(&frame[WIRE_PAYLOAD + 2u]) == expected_flags);
    assert(read_u32(&frame[WIRE_PAYLOAD + 4u]) == live_owner_status.compiled_default_digest);
    assert(read_u32(&frame[WIRE_PAYLOAD + 8u]) == live_owner_status.compiled_default_digest);
    assert(read_u32(&frame[WIRE_PAYLOAD + 12u]) == live_owner_status.active.payload_digest);
    assert(read_u32(&frame[WIRE_PAYLOAD + 16u]) == live_owner_status.pending.payload_digest);
    assert(read_u32(&frame[WIRE_PAYLOAD + 20u]) == live_owner_status.committed.payload_digest);
    assert(frame[WIRE_PAYLOAD + 24u] == NOAH_PROFILE_ACTIVE_COMMITTED);

    live_owner_status.active.generation        = 99u;
    live_owner_status.candidate.transaction_id = UINT16_C(0x6162);
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_STATUS, 0x42u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(read_u32(&frame[WIRE_PAYLOAD]) == 7u);
    assert(frame[WIRE_PAYLOAD + 4u] == 1u);
    assert(read_u32(&frame[WIRE_PAYLOAD + 5u]) == 7u);
    assert(frame[WIRE_PAYLOAD + 9u] == 1u);
    assert(read_u32(&frame[WIRE_PAYLOAD + 10u]) == 9u);
    assert(frame[WIRE_PAYLOAD + 14u] == 0u);
    assert(read_u16(&frame[WIRE_PAYLOAD + 15u]) == UINT16_C(0x4142));
    assert(read_u16(&frame[WIRE_PAYLOAD + 17u]) == UINT16_C(0x5152));
    assert(frame[WIRE_PAYLOAD + 21u] == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    assert(frame[WIRE_PAYLOAD + 22u] == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
}
#endif

static void test_hook_rejects_unsupported_or_malformed_commands(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 0u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[0] == id_custom_get_value);
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED);

    make_request(frame, 0x7Fu, 1u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[0] == id_unhandled);

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_CAPABILITY, 1u, 0u);
    via_custom_value_command_kb(frame, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 1u);
    assert(frame[0] == id_unhandled);

    memset(frame, 0, sizeof(frame));
    frame[0] = id_custom_set_value;
    frame[1] = id_custom_channel;
    frame[2] = NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN;
    via_custom_value_command_kb(frame, sizeof(frame));
#ifdef NOAH_LIVE_PROFILE_MUTATION_ENABLE
    assert(frame[0] == id_custom_set_value);
    assert(frame[5] == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED);
    assert(mutation_receive_count == 1u);
#else
    assert(frame[0] == id_unhandled);
#endif

    make_request(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_STATUS, 2u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
#ifdef NOAH_LIVE_PROFILE_MUTATION_ENABLE
    assert(frame[0] == id_custom_get_value);
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[WIRE_PAYLOAD + 1u] == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    assert(read_u16(&frame[WIRE_PAYLOAD + 4u]) == live_owner_status.candidate.transaction_id);
#else
    assert(frame[0] == id_unhandled);
#endif

    via_custom_value_command_kb(NULL, 0u);
}

#ifdef NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE
static void test_cadence_read_route_is_bounded_and_canonical(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    make_request(frame, NOAH_RUNTIME_CADENCE_WIRE_VALUE, 0x71u, 3u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[6] == NOAH_RUNTIME_CADENCE_WIRE_PAYLOAD_SIZE);
    assert(frame[WIRE_PAYLOAD] == 1u && frame[WIRE_PAYLOAD + 1u] == 3u);

    make_request(frame, NOAH_RUNTIME_CADENCE_WIRE_VALUE, 0x72u, NOAH_RUNTIME_CADENCE_WIRE_PAGES);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE);
    assert(frame[6] == 0u);

    make_request(frame, NOAH_RUNTIME_CADENCE_WIRE_VALUE, 0u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED);
}
#endif

// Committed-payload readback. The wire contract is: page 0 is metadata,
// pages 1..N are raw payload bytes, and anything past the payload is refused
// rather than served as zeros.
static void test_payload_read_reports_metadata_then_chunks(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    store_runtime_has_committed         = true;
    store_runtime_record.slot           = NOAH_PROFILE_SLOT_A;
    store_runtime_record.payload_length = 60u;
    store_runtime_record.generation     = 9u;
    store_runtime_record.payload_digest = UINT32_C(0xAABBCCDD);
    store_runtime_record.payload_crc32  = UINT32_C(0x11223344);
    store_runtime_record.schema_major   = 1u;
    store_runtime_record.schema_minor   = 2u;
    store_runtime_record.domain_mask    = 0x03u;
    store_runtime_record.origin_half    = 1u;

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD, 1u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[6] == NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE);
    assert(frame[7] == 1u);
    assert(read_u16(&frame[9]) == 60u);
    assert(read_u32(&frame[11]) == 9u);
    assert(read_u32(&frame[15]) == UINT32_C(0xAABBCCDD));
    assert(read_u32(&frame[19]) == UINT32_C(0x11223344));
    assert(frame[23] == 1u && frame[24] == 2u);
    assert(frame[25] == 0x03u && frame[26] == 1u);

    // Page 1 starts at offset 0 and carries a full report of payload.
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD, 2u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[6] == NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE);
    for (uint8_t index = 0u; index < NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE; index++) {
        assert(frame[7 + index] == store_payload_byte(index));
    }

    // The final chunk is short, and reports its real length.
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD, 3u, 3u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[6] == 10u);
    assert(frame[7] == store_payload_byte(50u));
}

static void test_payload_read_refuses_pages_past_the_payload(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    store_runtime_has_committed         = true;
    store_runtime_record.slot           = NOAH_PROFILE_SLOT_A;
    store_runtime_record.payload_length = 30u;

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD, 1u, 3u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE);
    assert(frame[6] == 0u);
}

static void test_payload_read_reports_unavailable_without_a_commit(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    store_runtime_has_committed = false;

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD, 1u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE);
}

static void test_payload_read_rejects_malformed_requests(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    store_runtime_has_committed         = true;
    store_runtime_record.slot           = NOAH_PROFILE_SLOT_A;
    store_runtime_record.payload_length = 60u;

    // A zero request id is not correlatable.
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD, 0u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED);

    // Reserved bytes must be zero, so a host cannot smuggle state in them.
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD, 1u, 1u);
    frame[20] = 0x5Au;
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED);
}

static void test_payload_read_surfaces_storage_failure(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    store_runtime_has_committed         = true;
    store_runtime_record.slot           = NOAH_PROFILE_SLOT_A;
    store_runtime_record.payload_length = 60u;
    store_read_committed_fails          = true;

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_PAYLOAD, 1u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE);
    store_read_committed_fails = false;
}

static void test_compiled_read_serves_what_the_firmware_runs(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    store_has_compiled          = true;
    store_runtime_has_committed = false; // nothing committed: compiled must still answer

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_COMPILED, 1u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(read_u16(&frame[9]) == 40u);
    // Compiled defaults are not a committed generation.
    assert(read_u32(&frame[11]) == 0u);
    assert(read_u32(&frame[15]) == UINT32_C(0x01020304));
    assert(read_u32(&frame[19]) == UINT32_C(0x05060708));
    assert(frame[25] == 0x30u);

    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_COMPILED, 2u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(frame[6] == NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE);
    assert(frame[7] == store_compiled_byte(0u));

    // The two sources must not be confused for one another.
    assert(frame[7] != store_payload_byte(0u));
}

static void test_compiled_read_reports_unavailable_when_absent(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    store_has_compiled = false;
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_COMPILED, 1u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE);
    store_has_compiled = true;
}

static void test_combo_read_route_without_combo_feature(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_COMBOS, 17u, 0u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[0] == id_custom_get_value && frame[3] == 17u);
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK && frame[6] == 25u);
    assert(frame[7] == 1u && frame[8] == 0u && frame[9] == 4u && frame[11] == 0u);
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_COMBOS, 18u, 1u);
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE && frame[6] == 0u);
    make_request(frame, NOAH_PROFILE_WIRE_V1_VALUE_COMBOS, 19u, 0u);
    frame[0] = id_custom_set_value;
    via_custom_value_command_kb(frame, sizeof(frame));
    assert(frame[0] == id_unhandled);
}

int main(void) {
#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
    initialize_live_owner_status();
#endif
    test_capability_hook();
#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
    test_status_surfaces_coherent_owner_snapshot();
#else
    test_status_surfaces_read_only_store_discovery();
#endif
    test_hook_rejects_unsupported_or_malformed_commands();
#ifdef NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE
    test_cadence_read_route_is_bounded_and_canonical();
#endif
    test_payload_read_reports_metadata_then_chunks();
    test_payload_read_refuses_pages_past_the_payload();
    test_payload_read_reports_unavailable_without_a_commit();
    test_payload_read_rejects_malformed_requests();
    test_payload_read_surfaces_storage_failure();
    test_compiled_read_serves_what_the_firmware_runs();
    test_compiled_read_reports_unavailable_when_absent();
    test_combo_read_route_without_combo_feature();
    puts("qmk VIA Profile Wire channel tests passed");
    return 0;
}
