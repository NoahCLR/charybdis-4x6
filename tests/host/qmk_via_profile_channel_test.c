#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "via.h"

#include "users/noah/lib/profile/protocol/profile_wire_v1.h"
#include "users/noah/lib/profile/storage/profile_storage_layout.h"
#include "users/noah/lib/profile/storage/profile_store_runtime.h"

void via_custom_value_command_kb(uint8_t *data, uint8_t length);

enum {
    WIRE_PAYLOAD = 7u,
};

static noah_profile_store_runtime_state_t store_runtime_state;
static noah_profile_store_record_t        store_runtime_record;
static bool                               store_runtime_has_committed;

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
    live_owner_status.pending                 = (noah_effective_profile_identity_t){
        .generation     = 8u,
        .payload_digest = UINT32_C(0x21222324),
        .origin         = 0u,
        .kind           = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE,
    };
    live_owner_status.has_pending             = true;
    live_owner_status.safe_boundary_reason_mask = 1u;
    live_owner_status.committed               = (noah_profile_split_descriptor_t){
        .generation     = 7u,
        .payload_digest = UINT32_C(0x31323334),
        .origin_half    = 1u,
        .readable       = true,
        .has_profile    = true,
    };
    live_owner_status.has_committed            = true;
    live_owner_status.candidate                = (noah_profile_candidate_v1_status_t){
        .state          = NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED,
        .transaction_id = UINT16_C(0x4142),
    };
    live_owner_status.candidate_pending        = true;
    live_owner_status.last_committed_transaction_id = UINT16_C(0x5152);
    live_owner_status.peer                     = (noah_profile_split_descriptor_t){
        .generation  = 9u,
        .origin_half = 0u,
        .readable    = true,
        .has_profile = true,
    };
    live_owner_status.peer_known               = true;
    live_owner_status.authority_state           = NOAH_PROFILE_SPLIT_AUTHORITY_PEER_NEWER;
}
#endif

noah_profile_store_runtime_state_t noah_profile_store_runtime_state(void) {
    return store_runtime_state;
}

const noah_profile_store_record_t *noah_profile_store_runtime_committed(void) {
    return store_runtime_has_committed ? &store_runtime_record : NULL;
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
    flags |= NOAH_PROFILE_FEATURE_CANDIDATE_WRITE | NOAH_PROFILE_FEATURE_PERSISTENT_COMMIT | NOAH_PROFILE_FEATURE_RUNTIME_ACTIVATION | NOAH_PROFILE_FEATURE_PEER_RECONCILIATION;
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
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];
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

    live_owner_status.active.generation = 99u;
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
    puts("qmk VIA Profile Wire channel tests passed");
    return 0;
}
