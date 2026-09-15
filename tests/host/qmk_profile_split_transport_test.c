#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transactions.h"
#include "users/noah/lib/compat/qmk_profile_split_transport.h"
#include "users/noah/lib/profile/schema/profile_blob_v1.h"

_Static_assert(NOAH_PROFILE_SPLIT_V1_FRAME_SIZE == 32u, "transport fixture requires the reviewed 32-byte profile frame");

static slave_callback_t registered_callback;
static int8_t            registered_id;
static uint8_t           registration_count;
static uint8_t           rpc_exec_count;
static int8_t            rpc_exec_id;
static uint8_t           rpc_request_size;
static uint8_t           rpc_response_size;
static uint8_t           rpc_request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
static bool              rpc_exec_result;
static uint8_t           peer_begin_count;

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

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    registered_id       = transaction_id;
    registered_callback = callback;
    registration_count++;
}

bool transaction_rpc_exec(int8_t transaction_id, uint8_t request_size, const void *request_data, uint8_t response_size, void *response_data) {
    rpc_exec_count++;
    rpc_exec_id       = transaction_id;
    rpc_request_size  = request_size;
    rpc_response_size = response_size;

    CHECK(request_data != NULL);
    CHECK(response_data != NULL);
    CHECK(request_size == sizeof(rpc_request));
    CHECK(response_size == sizeof(rpc_request));
    memcpy(rpc_request, request_data, sizeof(rpc_request));
    for (uint8_t index = 0u; index < response_size; index++) {
        ((uint8_t *)response_data)[index] = (uint8_t)(rpc_request[index] ^ 0xFFu);
    }
    return rpc_exec_result;
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor) {
    (void)peer;
    (void)descriptor;
    peer_begin_count++;
    return NOAH_PROFILE_PEER_STORE_BUSY;
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_begin_logical(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor, uint32_t via_generation, uint32_t via_digest) {
    (void)via_generation;
    (void)via_digest;
    return noah_profile_peer_store_backend_begin(peer, descriptor);
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_write(noah_profile_peer_store_backend_t *peer, uint32_t generation, uint32_t payload_digest, uint16_t offset, const uint8_t *bytes, uint8_t length) {
    (void)peer;
    (void)generation;
    (void)payload_digest;
    (void)offset;
    (void)bytes;
    (void)length;
    return NOAH_PROFILE_PEER_STORE_BUSY;
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_commit_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor) {
    (void)peer;
    (void)descriptor;
    return NOAH_PROFILE_PEER_STORE_BUSY;
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_prepare_durable_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor) {
    (void)peer;
    (void)descriptor;
    return NOAH_PROFILE_PEER_STORE_BUSY;
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_prepared_commit_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor) {
    (void)peer;
    (void)descriptor;
    return NOAH_PROFILE_PEER_STORE_BUSY;
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_step(noah_profile_peer_store_backend_t *peer, uint8_t byte_budget) {
    (void)peer;
    (void)byte_budget;
    return NOAH_PROFILE_PEER_STORE_BUSY;
}

noah_profile_peer_store_result_t noah_profile_peer_store_backend_abort(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor) {
    (void)peer;
    (void)descriptor;
    return NOAH_PROFILE_PEER_STORE_BUSY;
}

noah_profile_peer_store_state_t noah_profile_peer_store_backend_state(const noah_profile_peer_store_backend_t *peer) {
    (void)peer;
    return NOAH_PROFILE_PEER_STORE_IDLE;
}

uint16_t noah_profile_peer_store_backend_next_offset(const noah_profile_peer_store_backend_t *peer) {
    (void)peer;
    return 0u;
}

static noah_profile_split_descriptor_t compiled_descriptor(void) {
    return (noah_profile_split_descriptor_t){
        .compiled_default_digest = UINT32_C(0x11223344),
        .action_abi_digest       = UINT32_C(0x55667788),
        .schema_major            = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .readable                = true,
        .has_profile             = false,
    };
}

static noah_profile_split_descriptor_t committed_descriptor(void) {
    return (noah_profile_split_descriptor_t){
        .generation              = 7u,
        .payload_crc32           = UINT32_C(0xA1B2C3D4),
        .payload_digest          = UINT32_C(0x10203040),
        .compiled_default_digest = UINT32_C(0x11223344),
        .action_abi_digest       = UINT32_C(0x55667788),
        .payload_length          = NOAH_PROFILE_BLOB_V1_HEADER_SIZE,
        .schema_major            = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .origin_half             = 1u,
        .readable                = true,
        .has_profile             = true,
    };
}

static bool local_descriptor_read(void *context, noah_profile_split_descriptor_t *descriptor) {
    (void)context;
    CHECK(descriptor != NULL);
    *descriptor = compiled_descriptor();
    return true;
}

static bool local_payload_read(void *context, const noah_profile_split_descriptor_t *descriptor, uint16_t offset, uint8_t *bytes, uint8_t length) {
    (void)context;
    (void)descriptor;
    (void)offset;
    (void)bytes;
    (void)length;
    return false;
}

static bool unused_exchange(void *context, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    (void)context;
    (void)request;
    (void)response;
    return false;
}

static void init_reconciler(noah_profile_split_reconciler_t *reconciler, noah_profile_peer_store_backend_t *peer_store) {
    noah_profile_split_reconciler_config_t config = {
        .local_descriptor = local_descriptor_read,
        .local_read       = local_payload_read,
        .exchange         = unused_exchange,
        .peer_store       = peer_store,
    };

    memset(peer_store, 0, sizeof(*peer_store));
    noah_profile_split_reconciler_init(reconciler, &config);
    CHECK(noah_profile_split_reconciler_authority(reconciler) != NULL);
}

static void reset_transport_stubs(void) {
    noah_qmk_profile_split_transport_reset_for_test();
    registered_callback = NULL;
    registered_id       = -1;
    registration_count  = 0u;
    rpc_exec_count      = 0u;
    rpc_exec_id         = -1;
    rpc_request_size    = 0u;
    rpc_response_size   = 0u;
    rpc_exec_result     = true;
    peer_begin_count    = 0u;
    memset(rpc_request, 0, sizeof(rpc_request));
}

static bool reconciler_mailbox_pending(const noah_profile_split_reconciler_t *reconciler) {
    noah_profile_split_reconciler_status_t status;

    CHECK(noah_profile_split_reconciler_status(reconciler, &status));
    return status.mailbox_pending;
}

static void test_init_registers_exact_profile_transaction(void) {
    noah_profile_split_reconciler_t reconciler = {0};
    noah_profile_split_reconciler_t other_reconciler = {0};
    noah_profile_peer_store_backend_t peer_store;
    noah_profile_peer_store_backend_t other_peer_store;

    reset_transport_stubs();
    CHECK(!noah_qmk_profile_split_transport_init(NULL));
    CHECK(!noah_qmk_profile_split_transport_init(&reconciler));
    CHECK(registration_count == 0u);

    init_reconciler(&reconciler, &peer_store);
    CHECK(noah_qmk_profile_split_transport_init(&reconciler));
    CHECK(registration_count == 1u);
    CHECK(registered_id == PUT_PROFILE_SPLIT_SYNC);
    CHECK(registered_callback != NULL);
    CHECK(noah_qmk_profile_split_transport_init(&reconciler));
    CHECK(registration_count == 1u);
    init_reconciler(&other_reconciler, &other_peer_store);
    CHECK(!noah_qmk_profile_split_transport_init(&other_reconciler));
    CHECK(registration_count == 1u);
}

static void test_callback_forwards_and_reuses_cached_busy_response(void) {
    noah_profile_split_reconciler_t reconciler;
    noah_profile_peer_store_backend_t peer_store;
    noah_profile_split_v1_frame_t request = {
        .kind       = NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN,
        .status     = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .descriptor = committed_descriptor(),
    };
    noah_profile_split_v1_frame_t response;
    uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];

    reset_transport_stubs();
    init_reconciler(&reconciler, &peer_store);
    CHECK(noah_qmk_profile_split_transport_init(&reconciler));
    CHECK(noah_profile_split_v1_frame_encode(&request, request_wire));

    memset(response_wire, 0xA5, sizeof(response_wire));
    registered_callback(sizeof(request_wire), request_wire, sizeof(response_wire), response_wire);
    CHECK(reconciler_mailbox_pending(&reconciler));
    CHECK(memcmp(reconciler.mailbox_wire, request_wire, sizeof(request_wire)) == 0);
    CHECK(noah_profile_split_v1_frame_decode(response_wire, sizeof(response_wire), &response));
    CHECK(response.kind == NOAH_PROFILE_SPLIT_V1_ACK);
    CHECK(response.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY);
    CHECK(response.generation == request.descriptor.generation);
    CHECK(response.payload_digest == request.descriptor.payload_digest);
    CHECK(response.payload_length == request.descriptor.payload_length);

    CHECK(noah_profile_split_reconciler_scan(&reconciler, false, 0u));
    CHECK(peer_begin_count == 1u);
    CHECK(!reconciler_mailbox_pending(&reconciler));
    CHECK(reconciler.cached_response_valid);

    memset(response_wire, 0x5A, sizeof(response_wire));
    registered_callback(sizeof(request_wire), request_wire, sizeof(response_wire), response_wire);
    CHECK(noah_profile_split_v1_frame_decode(response_wire, sizeof(response_wire), &response));
    CHECK(response.kind == NOAH_PROFILE_SPLIT_V1_ACK);
    CHECK(response.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY);
    CHECK(peer_begin_count == 1u);
    CHECK(!reconciler_mailbox_pending(&reconciler));

    // A transient BUSY is observable for one exact retry, then scan releases
    // it so a later identical request can be admitted after contention clears.
    CHECK(!noah_profile_split_reconciler_scan(&reconciler, false, NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS));
    CHECK(!reconciler.cached_response_valid);
    registered_callback(sizeof(request_wire), request_wire, sizeof(response_wire), response_wire);
    CHECK(reconciler_mailbox_pending(&reconciler));
    CHECK(noah_profile_split_reconciler_scan(&reconciler, false, NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS * 2u));
    CHECK(peer_begin_count == 2u);
}

static void test_callback_rejects_invalid_sizes_without_leaking_output(void) {
    noah_profile_split_reconciler_t reconciler;
    noah_profile_peer_store_backend_t peer_store;
    noah_profile_split_v1_frame_t request = {
        .kind       = NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN,
        .status     = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .descriptor = committed_descriptor(),
    };
    uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE + 1u];

    reset_transport_stubs();
    init_reconciler(&reconciler, &peer_store);
    CHECK(noah_qmk_profile_split_transport_init(&reconciler));
    CHECK(noah_profile_split_v1_frame_encode(&request, request_wire));

    memset(response_wire, 0xA5, sizeof(response_wire));
    registered_callback((uint8_t)(sizeof(request_wire) - 1u), request_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, response_wire);
    for (uint8_t index = 0u; index < NOAH_PROFILE_SPLIT_V1_FRAME_SIZE; index++) {
        CHECK(response_wire[index] == 0u);
    }
    CHECK(!reconciler_mailbox_pending(&reconciler));

    memset(response_wire, 0x5A, sizeof(response_wire));
    registered_callback(sizeof(request_wire), request_wire, (uint8_t)(NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - 1u), response_wire);
    for (uint8_t index = 0u; index < NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - 1u; index++) {
        CHECK(response_wire[index] == 0u);
    }
    CHECK(response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - 1u] == 0x5Au);
    CHECK(!reconciler_mailbox_pending(&reconciler));

    memset(response_wire, 0x3C, sizeof(response_wire));
    registered_callback(sizeof(request_wire), NULL, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, response_wire);
    for (uint8_t index = 0u; index < NOAH_PROFILE_SPLIT_V1_FRAME_SIZE; index++) {
        CHECK(response_wire[index] == 0u);
    }
    registered_callback(sizeof(request_wire), request_wire, 0u, NULL);
}

static void test_exchange_uses_exact_bidirectional_frame(void) {
    uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];

    reset_transport_stubs();
    for (uint8_t index = 0u; index < sizeof(request); index++) {
        request[index] = index;
    }
    memset(response, 0, sizeof(response));

    CHECK(noah_qmk_profile_split_transport_exchange(NULL, request, response));
    CHECK(rpc_exec_count == 1u);
    CHECK(rpc_exec_id == PUT_PROFILE_SPLIT_SYNC);
    CHECK(rpc_request_size == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    CHECK(rpc_response_size == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    CHECK(memcmp(rpc_request, request, sizeof(request)) == 0);
    for (uint8_t index = 0u; index < sizeof(response); index++) {
        CHECK(response[index] == (uint8_t)(request[index] ^ 0xFFu));
    }

    CHECK(!noah_qmk_profile_split_transport_exchange(NULL, NULL, response));
    CHECK(!noah_qmk_profile_split_transport_exchange(NULL, request, NULL));
    CHECK(rpc_exec_count == 1u);

    rpc_exec_result = false;
    CHECK(!noah_qmk_profile_split_transport_exchange(NULL, request, response));
    CHECK(rpc_exec_count == 2u);
}

int main(void) {
    test_init_registers_exact_profile_transaction();
    test_callback_forwards_and_reuses_cached_busy_response();
    test_callback_rejects_invalid_sizes_without_leaking_output();
    test_exchange_uses_exact_bidirectional_frame();

    puts("qmk profile split transport host tests passed");
    return 0;
}
