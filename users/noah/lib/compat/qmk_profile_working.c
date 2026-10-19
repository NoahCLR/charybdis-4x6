// ───────────────────────────────────────────────────────────────────────────
// QMK Working Copy Boundary (D-F32)
// ───────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "qmk_profile_working.h"
#include "qmk_durable_io.h"

#include <string.h>

#include "../profile/storage/profile_checksum.h"

#if defined(MCU_RP) && defined(PROTOCOL_CHIBIOS)
#    include "hardware/structs/rosc.h"
#    include "hardware/structs/timer.h"

uint32_t noah_qmk_profile_working_session(void) {
    uint32_t session = timer_hw->timerawl;

    for (uint8_t bit = 0u; bit < 32u; bit++) {
        session = (session << 1u | session >> 31u) ^ (rosc_hw->randombit & 1u);
    }
    return session;
}
#else
uint32_t noah_qmk_profile_working_session(void) {
    static uint32_t boots;

    return 0x5EED0000u + ++boots;
}
#endif

static void put_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static uint32_t get_u32(const uint8_t *source) {
    return (uint32_t)source[0] | (uint32_t)source[1] << 8u | (uint32_t)source[2] << 16u | (uint32_t)source[3] << 24u;
}

static uint32_t record_crc(const uint8_t *bytes) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, 12u));
}

void noah_qmk_edit_mode_encode(const noah_profile_edit_mode_record_t *record, uint8_t bytes[NOAH_QMK_EDIT_MODE_RECORD_SIZE]) {
    memset(bytes, 0, NOAH_QMK_EDIT_MODE_RECORD_SIZE);
    bytes[0] = (uint8_t)NOAH_QMK_EDIT_MODE_MAGIC;
    bytes[1] = (uint8_t)(NOAH_QMK_EDIT_MODE_MAGIC >> 8u);
    bytes[2] = NOAH_QMK_EDIT_MODE_LAYOUT;
    bytes[3] = record->mode;
    put_u32(&bytes[8], record->counter);
    put_u32(&bytes[12], record_crc(bytes));
}

bool noah_qmk_edit_mode_decode(const uint8_t bytes[NOAH_QMK_EDIT_MODE_RECORD_SIZE], noah_profile_edit_mode_record_t *record) {
    if (bytes[0] != (uint8_t)NOAH_QMK_EDIT_MODE_MAGIC || bytes[1] != (uint8_t)(NOAH_QMK_EDIT_MODE_MAGIC >> 8u) || bytes[2] != NOAH_QMK_EDIT_MODE_LAYOUT || !noah_profile_edit_mode_valid(bytes[3]) || bytes[4] || bytes[5] || bytes[6] || bytes[7] || get_u32(&bytes[12]) != record_crc(bytes)) {
        return false;
    }
    record->mode    = bytes[3];
    record->counter = get_u32(&bytes[8]);
    return record->counter != 0u;
}

bool noah_qmk_edit_mode_choose(const uint8_t datablock[NOAH_QMK_EDIT_MODE_RECORD_SIZE * NOAH_QMK_EDIT_MODE_RECORDS], noah_profile_edit_mode_record_t *record, uint8_t *slot) {
    noah_profile_edit_mode_record_t candidate;
    bool                            found = false;

    for (uint8_t index = 0u; index < NOAH_QMK_EDIT_MODE_RECORDS; index++) {
        if (noah_qmk_edit_mode_decode(&datablock[index * NOAH_QMK_EDIT_MODE_RECORD_SIZE], &candidate) && (!found || candidate.counter > record->counter)) {
            *record = candidate;
            if (slot) {
                *slot = index;
            }
            found = true;
        }
    }
    return found;
}

#if !defined(NOAH_HOST_QMK_STUB) && defined(EECONFIG_USER_DATA_SIZE) && (EECONFIG_USER_DATA_SIZE) >= 32
#    include "eeconfig.h"
#    include "qmk_profile_flash.h"

_Static_assert(NOAH_QMK_EDIT_MODE_RECORD_SIZE * NOAH_QMK_EDIT_MODE_RECORDS <= EECONFIG_USER_DATA_SIZE, "both edit-mode records fit QMK's user datablock");

static bool read_datablock(uint8_t *datablock) {
    return eeconfig_is_user_datablock_valid() && eeconfig_read_user_datablock(datablock, 0u, NOAH_QMK_EDIT_MODE_RECORD_SIZE * NOAH_QMK_EDIT_MODE_RECORDS) == NOAH_QMK_EDIT_MODE_RECORD_SIZE * NOAH_QMK_EDIT_MODE_RECORDS;
}

static bool load(void *context, noah_profile_edit_mode_record_t *record) {
    uint8_t datablock[NOAH_QMK_EDIT_MODE_RECORD_SIZE * NOAH_QMK_EDIT_MODE_RECORDS];

    (void)context;
    return read_datablock(datablock) && noah_qmk_edit_mode_choose(datablock, record, NULL);
}

static bool store(void *context, const noah_profile_edit_mode_record_t *record) {
    uint8_t                         datablock[NOAH_QMK_EDIT_MODE_RECORD_SIZE * NOAH_QMK_EDIT_MODE_RECORDS];
    uint8_t                         bytes[NOAH_QMK_EDIT_MODE_RECORD_SIZE];
    noah_profile_edit_mode_record_t current;
    uint8_t                         slot = 1u;

    (void)context;
    if (!read_datablock(datablock)) {
        memset(datablock, 0, sizeof(datablock));
    }
    if (noah_qmk_edit_mode_choose(datablock, &current, &slot)) {
        slot ^= 1u;
    } else {
        slot = 0u;
    }
    noah_qmk_edit_mode_encode(record, bytes);
    if (eeconfig_update_user_datablock(bytes, (uint32_t)slot * NOAH_QMK_EDIT_MODE_RECORD_SIZE, sizeof(bytes)) != sizeof(bytes)) {
        return false;
    }
    // Read it back: only a record that reads as written counts.
    return read_datablock(datablock) && memcmp(&datablock[slot * NOAH_QMK_EDIT_MODE_RECORD_SIZE], bytes, sizeof(bytes)) == 0;
}

// QMK's EEPROM is wear-levelled flash: a write may compact, and erase, as a
// slot erase does, so it waits for the same quiet keyboard (D-F29).
static bool may_store(void *context) {
    (void)context;
    return noah_qmk_profile_flash_keyboard_quiet();
}

noah_profile_edit_mode_storage_t noah_qmk_edit_mode_storage(void) {
    return (noah_profile_edit_mode_storage_t){.load = load, .store = store, .may_store = may_store, .context = NULL};
}
#else
noah_profile_edit_mode_storage_t noah_qmk_edit_mode_storage(void) {
    return (noah_profile_edit_mode_storage_t){0};
}
#endif

#if defined(SPLIT_TRANSACTION_IDS_USER) && !defined(NOAH_HOST_QMK_STUB_NO_TRANSACTIONS)
#    include "transactions.h" // QMK
#    include "../profile/split/profile_working_sync.h"

_Static_assert(NOAH_PROFILE_WORKING_SYNC_FRAME_MAX <= RPC_M2S_BUFFER_SIZE, "the largest working-sync frame fits one split RPC");

static noah_qmk_profile_working_receive_fn working_receive;
static void                               *working_context;

static void working_rpc(uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    if (!(response && response_size >= 32u && working_receive && working_receive(working_context, request, request_size, response, response_size))) {
        if (response && response_size != 0u) {
            memset(response, 0, response_size);
        }
    }
}

bool noah_qmk_profile_working_transport_init(noah_qmk_profile_working_receive_fn receive, void *context) {
    if (!receive) {
        return false;
    }
    if (working_receive) {
        return working_receive == receive && working_context == context;
    }
    working_receive = receive;
    working_context = context;
    transaction_register_rpc(PUT_PROFILE_WORKING_SYNC, working_rpc);
    return true;
}

bool noah_qmk_profile_working_transport_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    (void)context;
    if (!request || !response || request_length < 32u || request_length > RPC_M2S_BUFFER_SIZE || response_length < 32u || response_length > RPC_S2M_BUFFER_SIZE) {
        return false;
    }
    // The durable-I/O grant repeats no step after one that used the link.
    noah_qmk_durable_io_note_split_rpc();
    return transaction_rpc_exec(PUT_PROFILE_WORKING_SYNC, request_length, request, response_length, response);
}

#    ifdef NOAH_PROFILE_SPLIT_TRANSPORT_TEST
void noah_qmk_profile_working_transport_reset_for_test(void) {
    working_receive = NULL;
    working_context = NULL;
}
#    endif
#else
bool noah_qmk_profile_working_transport_init(noah_qmk_profile_working_receive_fn receive, void *context) {
    (void)receive;
    (void)context;
    return false;
}

bool noah_qmk_profile_working_transport_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    (void)context;
    (void)request;
    (void)request_length;
    (void)response;
    (void)response_length;
    return false;
}
#endif
