#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/compat/qmk_via_logical_profile.h"
#include "users/noah/lib/profile/protocol/profile_candidate_v1.h"
#include "users/noah/lib/profile/protocol/profile_wire_v1.h"

static noah_qmk_via_logical_status_t fake_status;
static bool submit_result;
static uint16_t submitted_transaction;
static noah_qmk_via_sync_frame_t submitted_request;

uint16_t noah_qmk_via_storage_region_size(noah_qmk_via_sync_region_t region) {
    switch (region) {
        case NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG: return 2u;
        case NOAH_QMK_VIA_SYNC_REGION_KEYMAP: return 960u;
        case NOAH_QMK_VIA_SYNC_REGION_MACRO: return 7191u;
        default: return 0u;
    }
}

bool noah_qmk_via_logical_submit(uint16_t transaction_id, const noah_qmk_via_sync_frame_t *request) {
    submitted_transaction = transaction_id;
    submitted_request = *request;
    return submit_result;
}

bool noah_qmk_via_logical_status(noah_qmk_via_logical_status_t *status) {
    *status = fake_status;
    return true;
}

static void reset(void) {
    fake_status = (noah_qmk_via_logical_status_t){0};
    submit_result = true;
    submitted_transaction = 0u;
    submitted_request = (noah_qmk_via_sync_frame_t){0};
}

static void mutation_header(uint8_t frame[32], uint8_t value, uint16_t transaction_id) {
    memset(frame, 0, 32u);
    frame[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = value;
    frame[3] = (uint8_t)transaction_id;
    frame[4] = (uint8_t)(transaction_id >> 8u);
}

static void write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static void test_begin_is_queued_without_io(void) {
    uint8_t frame[32];
    reset();
    mutation_header(frame, NOAH_QMK_VIA_LOGICAL_VALUE_BEGIN, 0x1234u);
    write_u32(&frame[5], 9u);
    write_u32(&frame[9], UINT32_C(0x89abcdef));
    assert(noah_qmk_via_logical_profile_handle(frame, sizeof(frame)));
    assert(frame[5] == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED && frame[6] == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE && frame[7] == 0xffu);
    assert(submitted_transaction == 0x1234u);
    assert(submitted_request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_BEGIN);
    assert(submitted_request.generation == 9u && submitted_request.digest == UINT32_C(0x89abcdef));
}

static void test_sparse_chunk_preserves_identity(void) {
    uint8_t frame[32];
    reset();
    mutation_header(frame, NOAH_QMK_VIA_LOGICAL_VALUE_CHUNK, 7u);
    frame[5] = NOAH_QMK_VIA_SYNC_REGION_KEYMAP;
    frame[6] = 24u;
    frame[8] = 0xc0u;
    frame[9] = 0x03u;
    frame[10] = 3u;
    frame[11] = 0xa1u;
    frame[12] = 0xa2u;
    frame[13] = 0xa3u;
    write_u32(&frame[23], 10u);
    write_u32(&frame[27], UINT32_C(0x10203040));
    assert(noah_qmk_via_logical_profile_handle(frame, sizeof(frame)));
    assert(submitted_request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK);
    assert(submitted_request.offset == 24u && submitted_request.region_length == 960u && submitted_request.payload_length == 3u);
    assert(submitted_request.payload[2] == 0xa3u && submitted_request.generation == 10u && submitted_request.digest == UINT32_C(0x10203040));
}

static void test_status_is_canonical(void) {
    uint8_t frame[32] = {NOAH_PROFILE_WIRE_V1_COMMAND_GET, NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL, NOAH_QMK_VIA_LOGICAL_VALUE_STATUS, 5u};
    reset();
    fake_status = (noah_qmk_via_logical_status_t){
        .state = NOAH_QMK_VIA_LOGICAL_STAGED,
        .last_status = NOAH_QMK_VIA_SYNC_STATUS_OK,
        .transaction_id = 44u,
        .operation_sequence = 17u,
        .generation = 8u,
        .digest = UINT32_C(0x55667788),
    };
    assert(noah_qmk_via_logical_profile_handle(frame, sizeof(frame)));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK && frame[6] == 18u);
    assert(frame[7] == 1u && frame[8] == NOAH_QMK_VIA_LOGICAL_STAGED && frame[10] == 0u);
    assert(frame[11] == 44u && frame[13] == 17u && frame[15] == 8u);
}

static void test_busy_and_malformed_are_explicit(void) {
    uint8_t frame[32];
    reset();
    submit_result = false;
    mutation_header(frame, NOAH_QMK_VIA_LOGICAL_VALUE_VERIFY, 2u);
    write_u32(&frame[5], 1u);
    write_u32(&frame[9], 2u);
    assert(noah_qmk_via_logical_profile_handle(frame, sizeof(frame)));
    assert(frame[5] == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY && frame[6] == NOAH_PROFILE_CANDIDATE_V1_ERROR_MAILBOX_BUSY);
    mutation_header(frame, NOAH_QMK_VIA_LOGICAL_VALUE_BEGIN, 0u);
    assert(noah_qmk_via_logical_profile_handle(frame, sizeof(frame)));
    assert(frame[5] == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_MALFORMED);
}

static void test_internal_accept_is_not_a_host_command(void) {
    reset();
    assert(noah_qmk_via_logical_profile_accept(3u, 4u, 5u));
    assert(submitted_transaction == 3u && submitted_request.kind == NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ACCEPT);
}

int main(void) {
    test_begin_is_queued_without_io();
    test_sparse_chunk_preserves_identity();
    test_status_is_canonical();
    test_busy_and_malformed_are_explicit();
    test_internal_accept_is_not_a_host_command();
    puts("qmk VIA logical-profile channel tests passed");
    return 0;
}
