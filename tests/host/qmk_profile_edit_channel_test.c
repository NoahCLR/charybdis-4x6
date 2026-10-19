// Profile Wire live edits (D-F32): report layouts against a fake owner.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/compat/qmk_profile_edit_channel.h"
#include "users/noah/lib/compat/qmk_portable_profile.h"
#include "users/noah/lib/key/behavior/participation.h"
#include "users/noah/lib/profile/protocol/profile_wire_v1.h"
#include "users/noah/lib/profile/storage/profile_store_runtime.h"

static struct {
    uint8_t                       op;
    uint16_t                      transaction;
    uint32_t                      session, revision, insert_total, offset, crc;
    uint8_t                       splice_count, flags, index, mode;
    noah_profile_working_splice_t splice;
    uint8_t                       bytes[32];
    uint32_t                      count;
} call;
static noah_profile_working_result_t       next_result = NOAH_PROFILE_WORKING_OK;
static noah_profile_owner_working_status_t fake;
static uint8_t                             working[100];
static bool                                behaviors_override = true;
static bool                                combos_on          = true;

noah_profile_working_result_t noah_profile_store_runtime_edit_begin(uint16_t transaction_id, uint32_t session, uint32_t revision, uint8_t splice_count, uint32_t insert_total, uint8_t flags) {
    call = (typeof(call)){.op = 1, .transaction = transaction_id, .session = session, .revision = revision, .splice_count = splice_count, .insert_total = insert_total, .flags = flags};
    return next_result;
}
noah_profile_working_result_t noah_profile_store_runtime_edit_splice(uint16_t transaction_id, uint8_t index, const noah_profile_working_splice_t *splice) {
    call = (typeof(call)){.op = 2, .transaction = transaction_id, .index = index, .splice = *splice};
    return next_result;
}
noah_profile_working_result_t noah_profile_store_runtime_edit_data(uint16_t transaction_id, uint32_t offset, const uint8_t *bytes, uint32_t count) {
    call = (typeof(call)){.op = 3, .transaction = transaction_id, .offset = offset, .count = count};
    memcpy(call.bytes, bytes, count);
    return next_result;
}
noah_profile_working_result_t noah_profile_store_runtime_edit_commit(uint16_t transaction_id, uint32_t crc32) {
    call = (typeof(call)){.op = 4, .transaction = transaction_id, .crc = crc32};
    return next_result;
}
noah_profile_working_result_t noah_profile_store_runtime_edit_abort(uint16_t transaction_id) {
    call = (typeof(call)){.op = 5, .transaction = transaction_id};
    return next_result;
}
noah_profile_working_result_t noah_profile_store_runtime_save(uint16_t transaction_id, uint32_t session, uint32_t revision) {
    call = (typeof(call)){.op = 6, .transaction = transaction_id, .session = session, .revision = revision};
    return next_result;
}
noah_profile_working_result_t noah_profile_store_runtime_revert(uint16_t transaction_id, uint32_t session, uint32_t revision) {
    call = (typeof(call)){.op = 7, .transaction = transaction_id, .session = session, .revision = revision};
    return next_result;
}
noah_profile_working_result_t noah_profile_store_runtime_set_edit_mode(uint8_t mode) {
    call = (typeof(call)){.op = 8, .mode = mode};
    return next_result;
}
bool noah_profile_store_runtime_working_status(noah_profile_owner_working_status_t *status) {
    *status = fake;
    return true;
}
bool noah_profile_store_runtime_read_working(uint32_t offset, uint8_t *target, uint32_t length) {
    if (offset > sizeof(working) || length > sizeof(working) - offset) return false;
    memcpy(target, working + offset, length);
    return true;
}
uint32_t noah_setting(uint8_t id, uint32_t fallback) {
    (void)fallback;
    return id == NOAH_SETTING_COMBOS_ENABLED ? 1u : id == NOAH_SETTING_BEHAVIORS_ENABLED ? 0u : 0u;
}
bool noah_participation_behaviors_override(void) {
    return behaviors_override;
}
bool is_combo_enabled(void) {
    return combos_on;
}
uint32_t noah_qmk_portable_live_setting(uint8_t id) {
    return id == NOAH_SETTING_DEFAULT_DPI ? 1600u : id == NOAH_SETTING_RGB_COLOR ? 0x00C8FF55u : 0u;
}

static uint8_t frame[32];

static void set(uint8_t value, uint16_t transaction) {
    memset(frame, 0, sizeof(frame));
    frame[0] = 0x07u;
    frame[2] = value;
    frame[3] = (uint8_t)transaction;
    frame[4] = (uint8_t)(transaction >> 8u);
}

static void get(uint8_t value, uint16_t page) {
    memset(frame, 0, sizeof(frame));
    frame[0] = 0x08u;
    frame[2] = value;
    frame[3] = 0x5Au;
    frame[4] = (uint8_t)page;
    frame[5] = (uint8_t)(page >> 8u);
}

static void put_u32(uint8_t *target, uint32_t value) {
    for (uint8_t i = 0u; i < 4u; i++) target[i] = (uint8_t)(value >> (8u * i));
}

static uint32_t u32(const uint8_t *source) {
    return (uint32_t)source[0] | (uint32_t)source[1] << 8u | (uint32_t)source[2] << 16u | (uint32_t)source[3] << 24u;
}

static void test_sets(void) {
    // BEGIN: session, revision, count, flags, inserted bytes; the reply names
    // the result and where the working copy is.
    set(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_BEGIN, 0x1234u);
    put_u32(&frame[5], 0xAABBCCDDu);
    put_u32(&frame[9], 7u);
    frame[13] = 2u;
    frame[14] = 0u;
    put_u32(&frame[15], 300u);
    next_result = NOAH_PROFILE_WORKING_STALE;
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)));
    assert(call.op == 1 && call.transaction == 0x1234u && call.session == 0xAABBCCDDu && call.revision == 7u && call.splice_count == 2u && call.insert_total == 300u);
    assert(frame[0] == 0x07u && frame[3] == 0x34u && frame[4] == 0x12u && frame[5] == NOAH_PROFILE_WORKING_STALE);
    assert(frame[6] == fake.local.phase && u32(&frame[7]) == fake.local.session && u32(&frame[11]) == fake.local.revision);
    assert((frame[15] & NOAH_PROFILE_EDIT_WIRE_UNSAVED) != 0u);
    // A nonzero reserved byte or a zero transaction is refused unread.
    next_result = NOAH_PROFILE_WORKING_OK;
    call.op     = 0u;
    set(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_BEGIN, 1u);
    frame[31] = 1u;
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && call.op == 0u && frame[5] == NOAH_PROFILE_WORKING_INVALID_ARGUMENT);
    set(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_COMMIT, 0u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && call.op == 0u && frame[5] == NOAH_PROFILE_WORKING_INVALID_ARGUMENT);
    // SPLICE.
    set(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_SPLICE, 9u);
    frame[5] = 1u;
    frame[6] = 0x60u;
    frame[7] = 0u;
    put_u32(&frame[8], 1000u);
    put_u32(&frame[12], 2u);
    put_u32(&frame[16], 4u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)));
    assert(call.op == 2 && call.index == 1u && call.splice.domain_id == 0x60u && call.splice.offset == 1000u && call.splice.remove == 2u && call.splice.insert == 4u && frame[5] == NOAH_PROFILE_WORKING_OK);
    // DATA: up to 22 bytes, and nothing after them.
    set(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_DATA, 9u);
    put_u32(&frame[5], 40u);
    frame[9] = NOAH_PROFILE_EDIT_WIRE_DATA_MAX;
    for (uint8_t i = 0u; i < NOAH_PROFILE_EDIT_WIRE_DATA_MAX; i++) frame[10u + i] = (uint8_t)(0x80u + i);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)));
    assert(call.op == 3 && call.offset == 40u && call.count == NOAH_PROFILE_EDIT_WIRE_DATA_MAX && call.bytes[0] == 0x80u && call.bytes[21] == 0x95u);
    call.op = 0u;
    set(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_DATA, 9u);
    frame[9]  = 2u;
    frame[12] = 1u; // a byte past the count
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && call.op == 0u);
    set(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_DATA, 9u);
    frame[9] = NOAH_PROFILE_EDIT_WIRE_DATA_MAX + 1u;
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && call.op == 0u);
    // COMMIT, ABORT, SAVE, REVERT, mode.
    set(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_COMMIT, 9u);
    put_u32(&frame[5], 0xCAFEF00Du);
    next_result = NOAH_PROFILE_WORKING_QUEUED;
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && call.op == 4 && call.crc == 0xCAFEF00Du && frame[5] == NOAH_PROFILE_WORKING_QUEUED);
    set(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_ABORT, 9u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && call.op == 5 && call.transaction == 9u);
    set(NOAH_PROFILE_WIRE_V1_VALUE_SAVE, 10u);
    put_u32(&frame[5], 1u);
    put_u32(&frame[9], 2u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && call.op == 6 && call.session == 1u && call.revision == 2u);
    set(NOAH_PROFILE_WIRE_V1_VALUE_REVERT, 11u);
    put_u32(&frame[5], 3u);
    put_u32(&frame[9], 4u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && call.op == 7 && call.session == 3u && call.revision == 4u);
    set(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_MODE, 12u);
    frame[5] = NOAH_PROFILE_EDIT_MODE_APPLY;
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && call.op == 8 && call.mode == NOAH_PROFILE_EDIT_MODE_APPLY);
    // Other values are not this channel's.
    set(0x10u, 1u);
    assert(!noah_qmk_profile_edit_channel(frame, sizeof(frame)));
    next_result = NOAH_PROFILE_WORKING_OK;
    puts("edit SET reports decode exactly and answer with the working copy's identity");
}

static void test_gets(void) {
    get(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_STATUS, 0u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK && frame[6] == 25u);
    assert(frame[7] == NOAH_PROFILE_EDIT_WIRE_STATUS_LAYOUT && frame[8] == fake.local.phase && u32(&frame[11]) == fake.local.session && u32(&frame[15]) == fake.local.revision && u32(&frame[19]) == fake.saved_generation);
    assert(frame[29] == NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW);
    assert((frame[9] & NOAH_PROFILE_EDIT_WIRE_UNSAVED) && (frame[9] & NOAH_PROFILE_EDIT_WIRE_PEER_CONVERGED));
    get(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_STATUS, 1u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && u32(&frame[11]) == fake.peer.session && u32(&frame[15]) == fake.peer.revision);
    get(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_STATUS, 2u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && frame[7] == fake.local.error.code && frame[8] == fake.local.error.domain_id && u32(&frame[25]) == fake.local.length);
    get(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_STATUS, 3u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE);
    get(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_STATUS, 0u);
    frame[3] = 0u;
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED);

    // The working copy, paged like the saved profile.
    get(NOAH_PROFILE_WIRE_V1_VALUE_WORKING, 0u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && frame[5] == 0u && frame[7] == NOAH_PROFILE_EDIT_WIRE_WORKING_LAYOUT && frame[8] == 25u);
    assert(u32(&frame[9]) == fake.local.length && u32(&frame[13]) == fake.local.session && u32(&frame[17]) == fake.local.revision && frame[27] == 1u);
    for (uint16_t page = 1u; (uint32_t)(page - 1u) * 25u < fake.local.length; page++) {
        uint32_t offset = (uint32_t)(page - 1u) * 25u;
        uint32_t count  = fake.local.length - offset < 25u ? fake.local.length - offset : 25u;
        get(NOAH_PROFILE_WIRE_V1_VALUE_WORKING, page);
        assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && frame[5] == 0u && frame[6] == count && memcmp(&frame[7], working + offset, count) == 0);
    }
    get(NOAH_PROFILE_WIRE_V1_VALUE_WORKING, 5u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE);

    // Runtime overrides beside the stored masters, and QMK's live values.
    behaviors_override = false;
    combos_on          = false;
    get(NOAH_PROFILE_WIRE_V1_VALUE_RUNTIME, 0u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && frame[5] == 0u && frame[6] == 24u);
    assert(frame[7] == 1u && frame[8] == 0u && frame[9] == 0u && frame[10] == 0u);
    assert((frame[11] | frame[12] << 8) == 1600u && u32(&frame[19]) == 0x00C8FF55u);

    // The edit mode on both halves.
    get(NOAH_PROFILE_WIRE_V1_VALUE_EDIT_MODE, 0u);
    assert(noah_qmk_profile_edit_channel(frame, sizeof(frame)) && frame[6] == 11u && frame[7] == NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW && u32(&frame[8]) == 3u && frame[12] == NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW && u32(&frame[13]) == 3u && frame[17] == (1u | 4u));
    puts("edit status, working copy, runtime overrides and mode read back page by page");
}

int main(void) {
    for (uint8_t i = 0u; i < sizeof(working); i++) working[i] = (uint8_t)(i * 7u);
    fake.supported             = true;
    fake.local.session         = 0x01020304u;
    fake.local.revision        = 42u;
    fake.local.phase           = NOAH_PROFILE_WORKING_PHASE_IDLE;
    fake.local.active          = true;
    fake.local.length          = 80u;
    fake.local.domain_mask     = NOAH_PROFILE_DOMAIN_MASK_ALL;
    fake.local.error.code      = NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN;
    fake.local.error.domain_id = 0x60u;
    fake.saved_generation      = 5u;
    fake.saved_origin          = 1u;
    fake.peer.known            = true;
    fake.peer.session          = 0x01020304u;
    fake.peer.revision         = 42u;
    fake.peer.mode             = NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW;
    fake.peer.mode_counter     = 3u;
    fake.peer_required         = true;
    fake.peer_converged        = true;
    fake.mode                  = (noah_profile_edit_mode_record_t){.counter = 3u, .mode = NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW};
    test_sets();
    test_gets();
    return 0;
}
