#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transactions.h"
#include "users/noah/lib/compat/qmk_via_split_sync.h"
#include "users/noah/lib/compat/qmk_via_storage_contract.h"
#include "users/noah/lib/compat/qmk_via_storage_regions.h"
#include "users/noah/lib/compat/qmk_via_sync_metadata.h"
#include "users/noah/lib/compat/qmk_via_sync_protocol.h"
#include "users/noah/lib/compat/qmk_via_sync_state.h"

enum {
    TEST_CONFIG_SIZE = 2,
    TEST_KEYMAP_SIZE = 3,
    TEST_MACRO_SIZE  = 2,
};

typedef enum {
    TEST_RPC_EQUAL,
    TEST_RPC_PEER_DIRTY,
    TEST_RPC_PEER_NEWER,
    TEST_RPC_PEER_DIVERGED,
    TEST_RPC_DISCONNECTED,
} test_rpc_mode_t;

static bool             fake_master;
static uint32_t         fake_now;
static uint32_t         user_eeconfig_word;
static bool             fake_seed_succeeded;
static uint8_t          local_config[TEST_CONFIG_SIZE];
static uint8_t          local_keymap[TEST_KEYMAP_SIZE];
static uint8_t          local_macro[TEST_MACRO_SIZE];
static uint8_t          peer_config[TEST_CONFIG_SIZE];
static uint8_t          peer_keymap[TEST_KEYMAP_SIZE];
static uint8_t          peer_macro[TEST_MACRO_SIZE];
static uint8_t          rgb_invalidate_count;
static uint8_t          macro_invalidate_count;
static uint8_t          eeconfig_init_via_count;
static uint8_t          macro_defaults_init_count;
static uint16_t         rpc_count;
static uint8_t          rpc_commit_count;
static uint16_t         drop_rpc_at;
static bool             drop_after_apply;
// Models a peer that resets mid-transfer: it forgets its snapshot session and
// rejects further chunks until the master opens a new one with SNAPSHOT_BEGIN.
static uint16_t         peer_session_lost_at;
static bool             peer_session_active;
static uint16_t         peer_begin_count;
static test_rpc_mode_t  rpc_mode;
static slave_callback_t registered_callback;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

static uint32_t test_digest_parts(const uint8_t *config, const uint8_t *keymap, const uint8_t *macro) {
    uint32_t hash = UINT32_C(0x811C9DC5);

    for (uint8_t index = 0u; index < TEST_CONFIG_SIZE; index++) {
        hash = (hash ^ config[index]) * UINT32_C(16777619);
    }
    for (uint8_t index = 0u; index < TEST_KEYMAP_SIZE; index++) {
        hash = (hash ^ keymap[index]) * UINT32_C(16777619);
    }
    for (uint8_t index = 0u; index < TEST_MACRO_SIZE; index++) {
        hash = (hash ^ macro[index]) * UINT32_C(16777619);
    }
    return hash;
}

static uint32_t local_digest(void) {
    return test_digest_parts(local_config, local_keymap, local_macro);
}

static uint32_t peer_digest(void) {
    return test_digest_parts(peer_config, peer_keymap, peer_macro);
}

static void test_reset(void) {
    fake_master         = true;
    fake_now            = 0u;
    user_eeconfig_word  = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 5u});
    fake_seed_succeeded = true;
    local_config[0]     = 1u;
    local_config[1]     = 2u;
    local_keymap[0]     = 3u;
    local_keymap[1]     = 4u;
    local_keymap[2]     = 5u;
    local_macro[0]      = 6u;
    local_macro[1]      = 7u;
    memcpy(peer_config, local_config, sizeof(local_config));
    memcpy(peer_keymap, local_keymap, sizeof(local_keymap));
    memcpy(peer_macro, local_macro, sizeof(local_macro));
    rgb_invalidate_count      = 0u;
    macro_invalidate_count    = 0u;
    eeconfig_init_via_count   = 0u;
    macro_defaults_init_count = 0u;
    rpc_count                 = 0u;
    rpc_commit_count          = 0u;
    drop_rpc_at               = 0u;
    drop_after_apply          = false;
    peer_session_lost_at      = 0u;
    peer_session_active       = false;
    peer_begin_count          = 0u;
    rpc_mode                  = TEST_RPC_EQUAL;
    registered_callback       = NULL;
}

bool is_keyboard_master(void) {
    return fake_master;
}

uint32_t timer_read32(void) {
    return fake_now;
}

uint32_t eeconfig_read_user(void) {
    return user_eeconfig_word;
}

void eeconfig_update_user(uint32_t word) {
    user_eeconfig_word = word;
}

bool noah_via_macro_defaults_last_seed_succeeded(void) {
    return fake_seed_succeeded;
}

void via_macro_provider_invalidate_all(void) {
    macro_invalidate_count++;
}

void noah_rgb_runtime_invalidate_layer_maps(void) {
    rgb_invalidate_count++;
}

bool via_eeprom_is_valid(void) {
    return local_config[0] != 0u;
}

void via_eeprom_set_valid(bool valid) {
    local_config[0] = valid ? 1u : 0u;
}

void eeconfig_init_via(void) {
    eeconfig_init_via_count++;
    local_config[0] = 1u;
    local_config[1] = 0u;
    memset(local_keymap, 0, sizeof(local_keymap));
    memset(local_macro, 0, sizeof(local_macro));
}

bool noah_via_macro_defaults_reseed_for_recovery(void) {
    macro_defaults_init_count++;
    local_macro[0] = 0xA1u;
    local_macro[1] = 0xA2u;
    return fake_seed_succeeded;
}

uint16_t noah_qmk_via_storage_region_size(noah_qmk_via_sync_region_t region) {
    switch (region) {
        case NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG:
            return sizeof(local_config);
        case NOAH_QMK_VIA_SYNC_REGION_KEYMAP:
            return sizeof(local_keymap);
        case NOAH_QMK_VIA_SYNC_REGION_ENCODER:
            return 0u;
        case NOAH_QMK_VIA_SYNC_REGION_MACRO:
            return sizeof(local_macro);
        default:
            return 0u;
    }
}

static uint8_t *test_region_data(noah_qmk_via_sync_region_t region, bool peer) {
    switch (region) {
        case NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG:
            return peer ? peer_config : local_config;
        case NOAH_QMK_VIA_SYNC_REGION_KEYMAP:
            return peer ? peer_keymap : local_keymap;
        case NOAH_QMK_VIA_SYNC_REGION_MACRO:
            return peer ? peer_macro : local_macro;
        default:
            return NULL;
    }
}

bool noah_qmk_via_storage_region_read(noah_qmk_via_sync_region_t region, uint16_t offset, uint8_t *data, uint8_t length) {
    uint8_t *source   = test_region_data(region, false);
    uint16_t capacity = noah_qmk_via_storage_region_size(region);

    if (!source || !data || offset > capacity || length > capacity - offset) {
        return false;
    }
    memcpy(data, &source[offset], length);
    return true;
}

bool noah_qmk_via_storage_region_write(noah_qmk_via_sync_region_t region, uint16_t offset, const uint8_t *data, uint8_t length) {
    uint8_t *destination = test_region_data(region, false);
    uint16_t capacity    = noah_qmk_via_storage_region_size(region);

    if (!destination || !data || offset > capacity || length > capacity - offset) {
        return false;
    }
    memcpy(&destination[offset], data, length);
    return true;
}

void noah_qmk_via_storage_digest_init(noah_qmk_via_storage_digest_cursor_t *cursor) {
    CHECK(cursor != NULL);
    *cursor = (noah_qmk_via_storage_digest_cursor_t){0};
}

bool noah_qmk_via_storage_digest_step(noah_qmk_via_storage_digest_cursor_t *cursor, uint8_t byte_budget, uint32_t *out_digest) {
    CHECK(cursor != NULL);
    CHECK(byte_budget == NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX);
    CHECK(out_digest != NULL);
    if (cursor->offset == 0u) {
        cursor->offset = 1u;
        *out_digest    = 0u;
        return true;
    }
    cursor->complete = true;
    *out_digest      = local_digest();
    return true;
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    CHECK(transaction_id == PUT_VIA_KEYMAP_SYNC);
    registered_callback = callback;
}

static void encode_response(const noah_qmk_via_sync_frame_t *response, uint8_t size, void *data) {
    CHECK(size == NOAH_QMK_VIA_SYNC_FRAME_SIZE);
    CHECK(noah_qmk_via_sync_frame_encode(response, data));
}

bool transaction_rpc_exec(int8_t transaction_id, uint8_t request_size, const void *request_data, uint8_t response_size, void *response_data) {
    noah_qmk_via_sync_frame_t request;
    noah_qmk_via_sync_frame_t response;

    CHECK(transaction_id == PUT_VIA_KEYMAP_SYNC);
    CHECK(noah_qmk_via_sync_frame_decode(request_data, request_size, &request));
    rpc_count++;
    if (rpc_mode == TEST_RPC_DISCONNECTED) {
        return false;
    }
    if (drop_rpc_at == rpc_count && !drop_after_apply) {
        return false;
    }

    if (request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_METADATA) {
        response = (noah_qmk_via_sync_frame_t){
            .kind       = NOAH_QMK_VIA_SYNC_MESSAGE_METADATA,
            .status     = rpc_mode == TEST_RPC_PEER_DIRTY ? NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED : NOAH_QMK_VIA_SYNC_STATUS_OK,
            .generation = rpc_mode == TEST_RPC_PEER_NEWER ? 9u : request.generation,
            .digest     = (rpc_mode == TEST_RPC_PEER_NEWER || rpc_mode == TEST_RPC_PEER_DIVERGED) ? peer_digest() : request.digest,
        };
    } else if (request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN) {
        peer_session_active = true;
        peer_begin_count++;
        response = (noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_ACK, .generation = request.generation, .digest = request.digest};
    } else if (request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK) {
        uint8_t *destination;

        if (peer_session_lost_at != 0u && rpc_count >= peer_session_lost_at) {
            peer_session_active  = false;
            peer_session_lost_at = 0u;
        }
        if (!peer_session_active) {
            // Same reply the real receiver gives for an inactive session.
            response = (noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, .status = NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, .generation = request.generation, .digest = request.digest};
            encode_response(&response, response_size, response_data);
            return true;
        }
        destination = test_region_data(request.region, true);
        CHECK(destination != NULL);
        memcpy(&destination[request.offset], request.payload, request.payload_length);
        response = (noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_ACK, .region = request.region, .generation = request.generation, .offset = request.offset + request.payload_length, .region_length = request.region_length, .digest = request.digest};
    } else if (request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT) {
        rpc_commit_count++;
        response = (noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_ACK, .status = rpc_commit_count == 1u ? NOAH_QMK_VIA_SYNC_STATUS_BUSY : NOAH_QMK_VIA_SYNC_STATUS_OK, .generation = request.generation, .digest = request.digest};
    } else if (request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_PULL_CHUNK) {
        uint8_t *source    = test_region_data(request.region, true);
        uint16_t remaining = request.region_length - request.offset;
        CHECK(source != NULL);
        response = (noah_qmk_via_sync_frame_t){
            .kind           = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK,
            .region         = request.region,
            .generation     = request.generation,
            .offset         = request.offset,
            .region_length  = request.region_length,
            .digest         = request.digest,
            .payload_length = remaining < NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX ? (uint8_t)remaining : NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX,
        };
        memcpy(response.payload, &source[request.offset], response.payload_length);
    } else {
        CHECK(false);
    }

    if (drop_rpc_at == rpc_count && drop_after_apply) {
        return false;
    }
    encode_response(&response, response_size, response_data);
    return true;
}

static void scan_at(uint32_t now) {
    fake_now = now;
    noah_qmk_via_split_sync_matrix_scan();
}

static void scan_many(uint32_t start, uint16_t count) {
    for (uint16_t index = 0u; index < count; index++) {
        scan_at(start + index);
    }
}

static noah_qmk_via_sync_frame_t callback_exchange(noah_qmk_via_sync_frame_t request) {
    uint8_t                   request_wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE];
    uint8_t                   response_wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE] = {0};
    noah_qmk_via_sync_frame_t response;

    CHECK(registered_callback != NULL);
    CHECK(noah_qmk_via_sync_frame_encode(&request, request_wire));
    registered_callback(sizeof(request_wire), request_wire, sizeof(response_wire), response_wire);
    CHECK(noah_qmk_via_sync_frame_decode(response_wire, sizeof(response_wire), &response));
    return response;
}

static void test_receiver_returns_structured_error_for_corrupt_frame(void) {
    noah_qmk_via_sync_frame_t request = {.kind = NOAH_QMK_VIA_SYNC_MESSAGE_METADATA, .generation = 5u};
    noah_qmk_via_sync_frame_t response;
    uint8_t                   request_wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE];
    uint8_t                   response_wire[NOAH_QMK_VIA_SYNC_FRAME_SIZE] = {0};

    test_reset();
    fake_master = false;
    noah_qmk_via_split_sync_init();
    CHECK(noah_qmk_via_sync_frame_encode(&request, request_wire));
    request_wire[8] ^= 0x80u;
    registered_callback(sizeof(request_wire), request_wire, sizeof(response_wire), response_wire);
    CHECK(noah_qmk_via_sync_frame_decode(response_wire, sizeof(response_wire), &response));
    CHECK(response.kind == NOAH_QMK_VIA_SYNC_MESSAGE_ERROR);
    CHECK(response.status == NOAH_QMK_VIA_SYNC_STATUS_INVALID_FRAME);
    noah_qmk_via_split_sync_debug_snapshot_t debug = noah_qmk_via_split_sync_debug_snapshot();
    CHECK(debug.rejected_frame_count == 1u);
    CHECK(debug.last_error == NOAH_QMK_VIA_SYNC_STATUS_INVALID_FRAME);
}

static void test_equal_boot_handshake_and_periodic_refresh(void) {
    test_reset();
    noah_qmk_via_split_sync_init();
    scan_many(0u, 4u);
    CHECK(rpc_count == 1u);
    noah_qmk_via_split_sync_debug_snapshot_t debug = noah_qmk_via_split_sync_debug_snapshot();
    CHECK(debug.peer_generation == 5u);
    CHECK(debug.peer_digest == local_digest());
    CHECK(debug.last_peer_ack_generation == 5u);
    CHECK(debug.last_peer_ack_digest == local_digest());
    scan_at(999u);
    CHECK(rpc_count == 1u);
    scan_at(1002u);
    CHECK(rpc_count == 2u);
}

static void test_disconnect_uses_bounded_exponential_retry(void) {
    test_reset();
    rpc_mode = TEST_RPC_DISCONNECTED;
    noah_qmk_via_split_sync_init();
    scan_many(0u, 3u);
    CHECK(rpc_count == 1u);
    scan_at(49u);
    CHECK(rpc_count == 1u);
    scan_at(52u);
    CHECK(rpc_count == 2u);
    scan_at(151u);
    CHECK(rpc_count == 2u);
    scan_at(152u);
    CHECK(rpc_count == 3u);
    CHECK(noah_qmk_via_split_sync_debug_snapshot().retry_count == 3u);
}

static void test_local_mutation_commits_then_pushes_complete_snapshot(void) {
    noah_qmk_via_sync_metadata_t metadata;

    test_reset();
    rpc_mode = TEST_RPC_PEER_DIRTY;
    noah_qmk_via_split_sync_init();
    noah_qmk_via_split_sync_note_mutation(NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR | NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB);
    local_keymap[1] = 0xA5u;
    CHECK(noah_qmk_via_sync_metadata_decode(user_eeconfig_word, &metadata));
    CHECK(metadata.dirty);

    scan_many(0u, 20u);
    scan_at(70u);
    CHECK(noah_qmk_via_sync_metadata_decode(user_eeconfig_word, &metadata));
    CHECK(metadata.generation == 6u);
    CHECK(!metadata.dirty);
    CHECK(memcmp(peer_config, local_config, sizeof(local_config)) == 0);
    CHECK(memcmp(peer_keymap, local_keymap, sizeof(local_keymap)) == 0);
    CHECK(memcmp(peer_macro, local_macro, sizeof(local_macro)) == 0);
    CHECK(rpc_commit_count == 2u);
    noah_qmk_via_split_sync_debug_snapshot_t debug = noah_qmk_via_split_sync_debug_snapshot();
    CHECK(debug.last_peer_ack_generation == 6u);
    CHECK(debug.last_peer_ack_digest == local_digest());
}

static void test_each_outbound_boundary_recovers_from_loss(void) {
    for (uint16_t drop_at = 1u; drop_at <= 7u; drop_at++) {
        test_reset();
        rpc_mode         = TEST_RPC_PEER_DIRTY;
        drop_rpc_at      = drop_at;
        drop_after_apply = (drop_at & 1u) != 0u;
        noah_qmk_via_split_sync_init();
        local_keymap[1] = (uint8_t)(0xA0u + drop_at);
        noah_qmk_via_split_sync_note_mutation(NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR);

        scan_many(0u, 400u);

        CHECK(memcmp(peer_config, local_config, sizeof(local_config)) == 0);
        CHECK(memcmp(peer_keymap, local_keymap, sizeof(local_keymap)) == 0);
        CHECK(memcmp(peer_macro, local_macro, sizeof(local_macro)) == 0);
        CHECK(!noah_qmk_via_sync_state_snapshot().metadata.dirty);
    }
}

// A peer that resets mid-push forgets its snapshot session and rejects every
// further chunk with SNAPSHOT_REQUIRED. The master must abandon that session and
// open a new one, rather than re-sending the same chunk until something else
// happens to reset it.
static void test_peer_losing_snapshot_session_mid_push_renegotiates(void) {
    test_reset();
    rpc_mode = TEST_RPC_PEER_DIRTY;
    noah_qmk_via_split_sync_init();
    local_keymap[1] = 0x5Au;
    noah_qmk_via_split_sync_note_mutation(NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR);

    // Let a push get under way, then take the peer's session away mid-transfer.
    scan_many(0u, 600u);
    peer_session_lost_at = (uint16_t)(rpc_count + 1u);

    scan_many(600u, 8000u);

    // The loss is one-shot, so only a fresh SNAPSHOT_BEGIN can make the peer's
    // session active again. If the master merely retried the rejected chunk,
    // this stays false however long it runs.
    CHECK(peer_session_active);
    CHECK(memcmp(peer_keymap, local_keymap, sizeof(local_keymap)) == 0);
}

static void test_newer_peer_is_pulled_and_accepted(void) {
    noah_qmk_via_sync_metadata_t metadata;

    test_reset();
    rpc_mode       = TEST_RPC_PEER_NEWER;
    peer_config[1] = 0x44u;
    peer_keymap[0] = 0x55u;
    peer_macro[1]  = 0x66u;
    noah_qmk_via_split_sync_init();
    scan_many(0u, 20u);

    CHECK(memcmp(local_config, peer_config, sizeof(local_config)) == 0);
    CHECK(memcmp(local_keymap, peer_keymap, sizeof(local_keymap)) == 0);
    CHECK(memcmp(local_macro, peer_macro, sizeof(local_macro)) == 0);
    CHECK(noah_qmk_via_sync_metadata_decode(user_eeconfig_word, &metadata));
    CHECK(metadata.generation == 9u);
    CHECK(!metadata.dirty);
    CHECK(rgb_invalidate_count == 1u);
    CHECK(macro_invalidate_count == 1u);
    CHECK(macro_invalidate_count == 1u);
}

static void test_equal_generation_digest_conflict_makes_master_advance_and_push(void) {
    noah_qmk_via_sync_metadata_t metadata;

    test_reset();
    rpc_mode       = TEST_RPC_PEER_DIVERGED;
    peer_keymap[2] = 0xD1u;
    noah_qmk_via_split_sync_init();
    scan_many(0u, 20u);
    scan_at(70u);

    CHECK(noah_qmk_via_sync_metadata_decode(user_eeconfig_word, &metadata));
    CHECK(metadata.generation == 6u);
    CHECK(!metadata.dirty);
    CHECK(memcmp(peer_keymap, local_keymap, sizeof(local_keymap)) == 0);
    CHECK(rpc_commit_count == 2u);
    CHECK(noah_qmk_via_split_sync_debug_snapshot().conflict_count == 1u);
}

static void test_role_change_forces_new_metadata_session(void) {
    test_reset();
    fake_master = false;
    noah_qmk_via_split_sync_init();
    scan_many(0u, 5u);
    CHECK(rpc_count == 0u);

    fake_master = true;
    scan_at(10u);
    CHECK(rpc_count == 1u);
}

static void test_dirty_reboot_pulls_clean_peer(void) {
    noah_qmk_via_sync_metadata_t metadata;

    test_reset();
    user_eeconfig_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 5u, .dirty = true});
    rpc_mode           = TEST_RPC_PEER_NEWER;
    peer_keymap[0]     = 0xB1u;
    noah_qmk_via_split_sync_init();
    scan_many(0u, 20u);

    CHECK(local_keymap[0] == 0xB1u);
    CHECK(noah_qmk_via_sync_metadata_decode(user_eeconfig_word, &metadata));
    CHECK(metadata.generation == 9u);
    CHECK(!metadata.dirty);
}

static void test_mutation_during_dirty_recovery_does_not_block_reconciliation(void) {
    test_reset();
    user_eeconfig_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 5u, .dirty = true});
    rpc_mode           = TEST_RPC_PEER_NEWER;
    peer_keymap[0]     = 0xC1u;
    noah_qmk_via_split_sync_init();

    local_keymap[0] = 0xD1u;
    noah_qmk_via_split_sync_note_mutation(NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR);
    scan_many(0u, 30u);

    CHECK(rpc_count > 0u);
    CHECK(local_keymap[0] == 0xC1u);
    CHECK(noah_qmk_via_sync_state_snapshot().metadata.generation == 9u);
    CHECK(!noah_qmk_via_sync_state_snapshot().metadata.dirty);
}

static void test_two_dirty_halves_reseed_current_master_before_authority(void) {
    test_reset();
    user_eeconfig_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 5u, .dirty = true});
    rpc_mode           = TEST_RPC_PEER_DIRTY;
    noah_qmk_via_split_sync_init();
    scan_many(0u, 6u);

    CHECK(eeconfig_init_via_count == 1u);
    CHECK(macro_defaults_init_count == 1u);
    CHECK(noah_qmk_via_sync_state_snapshot().metadata.generation == 1u);
    CHECK(!noah_qmk_via_sync_state_snapshot().metadata.dirty);
    CHECK(local_macro[0] == 0xA1u);
    CHECK(local_macro[1] == 0xA2u);
    CHECK(rgb_invalidate_count == 1u);
    CHECK(macro_invalidate_count == 1u);
}

static void test_failed_macro_reseed_keeps_generation_dirty_and_sends_nothing(void) {
    test_reset();
    fake_seed_succeeded = false;
    noah_qmk_via_split_sync_init();
    noah_qmk_via_split_sync_note_mutation(NOAH_QMK_VIA_COMMAND_EFFECT_RESEED_MACROS | NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR);
    scan_many(0u, 10u);

    CHECK(noah_qmk_via_sync_state_snapshot().metadata.dirty);
    CHECK(rpc_count == 0u);

    fake_seed_succeeded = true;
    scan_many(10u, 4u);
    CHECK(!noah_qmk_via_sync_state_snapshot().metadata.dirty);
    CHECK(rpc_count > 0u);
}

static void test_two_dirty_recovery_seed_failure_stays_non_authoritative(void) {
    test_reset();
    user_eeconfig_word  = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 5u, .dirty = true});
    rpc_mode            = TEST_RPC_PEER_DIRTY;
    fake_seed_succeeded = false;
    noah_qmk_via_split_sync_init();
    scan_many(0u, 20u);

    CHECK(eeconfig_init_via_count == 1u);
    CHECK(macro_defaults_init_count == 1u);
    CHECK(noah_qmk_via_sync_state_snapshot().metadata.dirty);
    CHECK(noah_qmk_via_sync_state_snapshot().recovery_required);
    CHECK(rgb_invalidate_count == 0u);
    CHECK(macro_invalidate_count == 0u);

    fake_seed_succeeded = true;
    scan_many(60u, 10u);
    CHECK(eeconfig_init_via_count == 2u);
    CHECK(macro_defaults_init_count == 2u);
    CHECK(!noah_qmk_via_sync_state_snapshot().metadata.dirty);
    CHECK(rgb_invalidate_count == 1u);
    CHECK(macro_invalidate_count == 1u);
}

static void test_receiver_validates_order_accepts_duplicate_and_acks_after_digest(void) {
    noah_qmk_via_sync_frame_t response;
    uint32_t                  source_digest;

    test_reset();
    fake_master = false;
    noah_qmk_via_split_sync_init();
    scan_many(0u, 2u);

    peer_config[1] = 0x91u;
    peer_keymap[0] = 0x92u;
    peer_macro[0]  = 0x93u;
    source_digest  = peer_digest();

    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN, .generation = 7u, .digest = source_digest});
    CHECK(response.kind == NOAH_QMK_VIA_SYNC_MESSAGE_ACK);

    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_KEYMAP, .generation = 7u, .offset = 1u, .region_length = TEST_KEYMAP_SIZE, .digest = source_digest, .payload_length = 1u, .payload = {0xEEu}});
    CHECK(response.kind == NOAH_QMK_VIA_SYNC_MESSAGE_ERROR);
    CHECK(response.status == NOAH_QMK_VIA_SYNC_STATUS_RANGE_ERROR);

    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_KEYMAP, .generation = 7u, .region_length = TEST_KEYMAP_SIZE, .digest = source_digest, .payload_length = TEST_KEYMAP_SIZE, .payload = {peer_keymap[0], peer_keymap[1], peer_keymap[2]}});
    CHECK(response.kind == NOAH_QMK_VIA_SYNC_MESSAGE_ACK);
    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_KEYMAP, .generation = 7u, .region_length = TEST_KEYMAP_SIZE, .digest = source_digest, .payload_length = TEST_KEYMAP_SIZE, .payload = {peer_keymap[0], peer_keymap[1], peer_keymap[2]}});
    CHECK(response.kind == NOAH_QMK_VIA_SYNC_MESSAGE_ACK);

    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_MACRO, .generation = 7u, .region_length = TEST_MACRO_SIZE, .digest = source_digest, .payload_length = TEST_MACRO_SIZE, .payload = {peer_macro[0], peer_macro[1]}});
    CHECK(response.kind == NOAH_QMK_VIA_SYNC_MESSAGE_ACK);
    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG, .generation = 7u, .region_length = TEST_CONFIG_SIZE, .digest = source_digest, .payload_length = TEST_CONFIG_SIZE, .payload = {peer_config[0], peer_config[1]}});
    CHECK(response.kind == NOAH_QMK_VIA_SYNC_MESSAGE_ACK);

    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT, .generation = 7u, .digest = source_digest});
    CHECK(response.status == NOAH_QMK_VIA_SYNC_STATUS_BUSY);
    scan_many(10u, 2u);
    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT, .generation = 7u, .digest = source_digest});
    CHECK(response.status == NOAH_QMK_VIA_SYNC_STATUS_OK);
    CHECK(noah_qmk_via_sync_state_snapshot().metadata.generation == 7u);
    CHECK(!noah_qmk_via_sync_state_snapshot().metadata.dirty);
}

static void test_receiver_digest_mismatch_never_publishes_clean_generation(void) {
    noah_qmk_via_sync_frame_t response;
    uint32_t                  wrong_digest;

    test_reset();
    fake_master  = false;
    wrong_digest = peer_digest() ^ 1u;
    noah_qmk_via_split_sync_init();
    scan_many(0u, 2u);

    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN, .generation = 8u, .digest = wrong_digest});
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_KEYMAP, .generation = 8u, .region_length = TEST_KEYMAP_SIZE, .digest = wrong_digest, .payload_length = TEST_KEYMAP_SIZE, .payload = {peer_keymap[0], peer_keymap[1], peer_keymap[2]}});
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_MACRO, .generation = 8u, .region_length = TEST_MACRO_SIZE, .digest = wrong_digest, .payload_length = TEST_MACRO_SIZE, .payload = {peer_macro[0], peer_macro[1]}});
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG, .generation = 8u, .region_length = TEST_CONFIG_SIZE, .digest = wrong_digest, .payload_length = TEST_CONFIG_SIZE, .payload = {peer_config[0], peer_config[1]}});
    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT, .generation = 8u, .digest = wrong_digest});
    CHECK(response.status == NOAH_QMK_VIA_SYNC_STATUS_BUSY);
    scan_many(10u, 2u);
    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT, .generation = 8u, .digest = wrong_digest});
    CHECK(response.kind == NOAH_QMK_VIA_SYNC_MESSAGE_ERROR);
    CHECK(noah_qmk_via_sync_state_snapshot().metadata.dirty);
    CHECK(rgb_invalidate_count == 0u);
    CHECK(macro_invalidate_count == 0u);
    noah_qmk_via_split_sync_debug_snapshot_t debug = noah_qmk_via_split_sync_debug_snapshot();
    CHECK(debug.last_error == NOAH_QMK_VIA_SYNC_STATUS_DIGEST_MISMATCH);
    CHECK(!debug.receiver_active);
}

static void test_replacement_snapshot_restarts_in_progress_verification(void) {
    noah_qmk_via_sync_frame_t response;
    uint32_t                  first_digest;
    uint32_t                  replacement_digest;

    test_reset();
    fake_master = false;
    noah_qmk_via_split_sync_init();
    scan_many(0u, 2u);

    peer_keymap[0] = 0x31u;
    first_digest   = peer_digest();
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN, .generation = 7u, .digest = first_digest});
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_KEYMAP, .generation = 7u, .region_length = TEST_KEYMAP_SIZE, .digest = first_digest, .payload_length = TEST_KEYMAP_SIZE, .payload = {peer_keymap[0], peer_keymap[1], peer_keymap[2]}});
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_MACRO, .generation = 7u, .region_length = TEST_MACRO_SIZE, .digest = first_digest, .payload_length = TEST_MACRO_SIZE, .payload = {peer_macro[0], peer_macro[1]}});
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG, .generation = 7u, .region_length = TEST_CONFIG_SIZE, .digest = first_digest, .payload_length = TEST_CONFIG_SIZE, .payload = {peer_config[0], peer_config[1]}});
    response = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT, .generation = 7u, .digest = first_digest});
    CHECK(response.status == NOAH_QMK_VIA_SYNC_STATUS_BUSY);
    scan_at(10u);

    peer_keymap[0]     = 0x41u;
    peer_macro[1]      = 0x42u;
    replacement_digest = peer_digest();
    response           = callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN, .generation = 8u, .digest = replacement_digest});
    CHECK(response.status == NOAH_QMK_VIA_SYNC_STATUS_OK);
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_KEYMAP, .generation = 8u, .region_length = TEST_KEYMAP_SIZE, .digest = replacement_digest, .payload_length = TEST_KEYMAP_SIZE, .payload = {peer_keymap[0], peer_keymap[1], peer_keymap[2]}});
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_MACRO, .generation = 8u, .region_length = TEST_MACRO_SIZE, .digest = replacement_digest, .payload_length = TEST_MACRO_SIZE, .payload = {peer_macro[0], peer_macro[1]}});
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, .region = NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG, .generation = 8u, .region_length = TEST_CONFIG_SIZE, .digest = replacement_digest, .payload_length = TEST_CONFIG_SIZE, .payload = {peer_config[0], peer_config[1]}});
    (void)callback_exchange((noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT, .generation = 8u, .digest = replacement_digest});
    scan_many(20u, 2u);

    CHECK(noah_qmk_via_sync_state_snapshot().metadata.generation == 8u);
    CHECK(!noah_qmk_via_sync_state_snapshot().metadata.dirty);
    CHECK(local_keymap[0] == 0x41u);
    CHECK(local_macro[1] == 0x42u);
}

int main(void) {
    test_receiver_returns_structured_error_for_corrupt_frame();
    test_equal_boot_handshake_and_periodic_refresh();
    test_disconnect_uses_bounded_exponential_retry();
    test_local_mutation_commits_then_pushes_complete_snapshot();
    test_each_outbound_boundary_recovers_from_loss();
    test_peer_losing_snapshot_session_mid_push_renegotiates();
    test_newer_peer_is_pulled_and_accepted();
    test_equal_generation_digest_conflict_makes_master_advance_and_push();
    test_role_change_forces_new_metadata_session();
    test_dirty_reboot_pulls_clean_peer();
    test_mutation_during_dirty_recovery_does_not_block_reconciliation();
    test_two_dirty_halves_reseed_current_master_before_authority();
    test_failed_macro_reseed_keeps_generation_dirty_and_sends_nothing();
    test_two_dirty_recovery_seed_failure_stays_non_authoritative();
    test_receiver_validates_order_accepts_duplicate_and_acks_after_digest();
    test_receiver_digest_mismatch_never_publishes_clean_generation();
    test_replacement_snapshot_restarts_in_progress_verification();

    puts("qmk_via_split_sync host tests passed");
    return 0;
}
