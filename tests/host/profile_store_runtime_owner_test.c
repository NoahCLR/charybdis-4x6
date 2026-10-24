// The profile store runtime as the owner build links it: one half, with its
// grant, its slot flash and both split RPCs, driven through the runtime's
// interface. QMK, the physical half and the slot flash are faked below; the
// owner's runtimes and the compiled defaults are stubbed as the owner tests
// stub them.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "transactions.h"
#include "users/noah/lib/compat/qmk_durable_io.h"
#include "users/noah/lib/compat/qmk_physical_half.h"
#include "users/noah/lib/compat/qmk_profile_flash.h"
#include "users/noah/lib/compat/qmk_profile_split_transport.h"
#include "users/noah/lib/profile/runtime/profile_action_placement_v1.h"
#include "users/noah/lib/profile/schema/profile_pd_v1.h"
#include "users/noah/lib/profile/split/profile_split_protocol_v1.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/storage/profile_store_runtime.h"

enum {
    TRANSACTION_IDS = PUT_KEYBOARD_RESTART + 1,
    // Enough scans for the owner to validate the compiled defaults and read
    // both slots of an empty flash.
    BOOT_SCANS = 256u,
};

// Every profile the keyboard runs carries its pointing slots (the PD domain);
// this one carries it with every slot disabled.
static const uint8_t compiled_blob[] = {
    'N', 'L', 'P', '1', 4u, 0u, 1u, 1u,
    NOAH_PROFILE_DOMAIN_V1_PD, NOAH_PROFILE_DOMAIN_VERSION_PD, NOAH_PROFILE_PD_V1_HEADER_SIZE, 0u, 0u, 0u,
    NOAH_PROFILE_PD_V1_VERSION, NOAH_PROFILE_PD_V1_SLOT_COUNT, NOAH_PROFILE_PD_V1_RECORD_SIZE, 0u, 0u, 0u, 0u, 0u,
};

// ── The compiled defaults and the owner's runtimes, stubbed ─────────────────

static uint32_t crc_of(const uint8_t *bytes, size_t length) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length));
}

static bool compiled_read(void *context, size_t offset, uint8_t *target, size_t length) {
    (void)context;
    if (!target || length == 0u || offset > sizeof(compiled_blob) || length > sizeof(compiled_blob) - offset) {
        return false;
    }
    memcpy(target, &compiled_blob[offset], length);
    return true;
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_open(noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_error_t *error) {
    (void)error;
    if (!profile) return NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT;
    *profile = (noah_profile_compiled_v1_t){
        .metadata =
            {
                .crc32             = crc_of(compiled_blob, sizeof(compiled_blob)),
                .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, compiled_blob, sizeof(compiled_blob)),
                .action_abi_digest = UINT32_C(0x12345678),
                .byte_length       = sizeof(compiled_blob),
                .domain_mask       = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD,
            },
    };
    return NOAH_PROFILE_COMPILED_V1_OK;
}

noah_profile_reader_t noah_profile_compiled_v1_reader(const noah_profile_compiled_v1_t *profile) {
    return (noah_profile_reader_t){.read = compiled_read, .context = (void *)profile, .length = sizeof(compiled_blob)};
}

bool noah_profile_action_placement_v1_supported(const noah_profile_action_v1_t *action, noah_profile_validator_v1_placement_t placement) {
    (void)action;
    (void)placement;
    return true;
}

bool noah_profile_compiled_v1_compatibility(const noah_profile_compiled_v1_t *profile, noah_profile_validator_v1_compatibility_t *compatibility) {
    if (!profile || !compatibility) return false;
    // As the keyboard's own: every profile must carry the PD domain.
    *compatibility = noah_profile_validator_v1_default_compatibility(profile->metadata.action_abi_digest);
    return true;
}

void noah_active_profile_key_behavior_runtime_init(noah_active_profile_key_behavior_runtime_t *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->initialized = true;
}

bool noah_active_profile_key_behavior_runtime_install(noah_active_profile_key_behavior_runtime_t *runtime) {
    return runtime && runtime->initialized;
}

void noah_active_profile_key_behavior_runtime_uninstall(noah_active_profile_key_behavior_runtime_t *runtime) {
    (void)runtime;
}

void noah_active_profile_key_behavior_runtime_invalidate(void *context, uint32_t publication_count, noah_active_profile_identity_t previous, noah_active_profile_identity_t active, const noah_active_profile_snapshot_t *callback_view) {
    (void)context;
    (void)publication_count;
    (void)previous;
    (void)active;
    (void)callback_view;
}

void noah_active_profile_rgb_runtime_init(noah_active_profile_rgb_runtime_t *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->banks[0].valid = runtime->banks[1].valid = true;
    runtime->initialized = true;
}

bool noah_active_profile_rgb_runtime_install(noah_active_profile_rgb_runtime_t *runtime) {
    return runtime && runtime->initialized;
}

void noah_active_profile_rgb_runtime_uninstall(noah_active_profile_rgb_runtime_t *runtime) {
    (void)runtime;
}

void noah_active_profile_rgb_runtime_invalidate(void *context, uint32_t publication_count, noah_active_profile_identity_t previous, noah_active_profile_identity_t active, const noah_active_profile_snapshot_t *callback_view) {
    (void)context;
    (void)publication_count;
    (void)previous;
    (void)active;
    (void)callback_view;
}

void noah_profile_activation_policy_init(noah_profile_activation_policy_t *policy, noah_profile_activation_other_half_observer_fn observer, void *observer_context) {
    memset(policy, 0, sizeof(*policy));
    policy->other_half_observer = observer;
    policy->other_half_context = observer_context;
    policy->initialized   = true;
}

uint32_t noah_profile_activation_policy_safe_boundary(void *context) {
    (void)context;
    return 0u;
}

// ── The half, faked ─────────────────────────────────────────────────────────

static bool     half_known = true;
static bool     master;
static bool     keys_busy;
static uint32_t scan_now;

bool noah_qmk_physical_half_origin(uint8_t *origin) {
    *origin = NOAH_PHYSICAL_HALF_ORIGIN_LEFT;
    return half_known;
}

bool is_keyboard_master(void) {
    return master;
}

uint32_t timer_read32(void) {
    return scan_now;
}

uint32_t timer_elapsed32(uint32_t last) {
    return scan_now - last;
}

// A key held: the keyboard is not quiet, so no erase runs.
uint32_t last_matrix_activity_elapsed(void) {
    return keys_busy ? 0u : 1000u;
}

matrix_row_t matrix_get_row(uint8_t row) {
    (void)row;
    return 0u;
}

bool noah_qmk_profile_flash_eeprom_valid(void) {
    return true;
}

// The half's slot flash, NOR: erased bytes are 0xFF and programs only clear
// bits.
static struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_REGION_SIZE];
    uint32_t reads, erases, programs;
} flash;

static void flash_read(void *context, uint32_t offset, uint8_t *target, uint32_t length) {
    (void)context;
    assert(offset + length <= sizeof(flash.bytes));
    flash.reads++;
    memcpy(target, &flash.bytes[offset], length);
}

static bool flash_erase(void *context, uint32_t offset) {
    (void)context;
    assert(offset % NOAH_PROFILE_STORAGE_SECTOR_SIZE == 0u && offset < sizeof(flash.bytes));
    flash.erases++;
    memset(&flash.bytes[offset], 0xFF, NOAH_PROFILE_STORAGE_SECTOR_SIZE);
    return true;
}

static bool flash_program(void *context, uint32_t offset, const uint8_t *page) {
    (void)context;
    assert(offset % NOAH_PROFILE_STORAGE_PAGE_SIZE == 0u && offset < sizeof(flash.bytes));
    flash.programs++;
    for (uint32_t index = 0u; index < NOAH_PROFILE_STORAGE_PAGE_SIZE; index++) {
        flash.bytes[offset + index] &= page[index];
    }
    return true;
}

noah_profile_flash_driver_t noah_qmk_profile_flash_driver(void) {
    return (noah_profile_flash_driver_t){.erase_sector = flash_erase, .program_page = flash_program, .read = flash_read};
}

// QMK's split RPCs: what is registered. The link is down, so no request
// reaches the other half.
static slave_callback_t registered[TRANSACTION_IDS];
static unsigned         registrations[TRANSACTION_IDS];

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    assert(transaction_id >= 0 && transaction_id < TRANSACTION_IDS && callback);
    registered[transaction_id] = callback;
    registrations[transaction_id]++;
}

// Requests this half sent: only the master half (is_keyboard_master) sends.
static unsigned rpcs_sent;

bool transaction_rpc_exec(int8_t transaction_id, uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    rpcs_sent++;
    (void)transaction_id;
    (void)request_size;
    (void)request;
    (void)response_size;
    (void)response;
    return false;
}

static unsigned total_registrations(void) {
    unsigned total = 0u;

    for (int8_t id = 0; id < TRANSACTION_IDS; id++) total += registrations[id];
    return total;
}

// A boot of the half: empty flash, nothing registered, and the runtime
// started afresh. QMK registers RPCs once per boot, so the transport forgets
// its registration too. The slot flash store has no reset and keeps its RAM;
// no test leaves it a sector to erase.
static void boot(void) {
    memset(&flash, 0, sizeof(flash));
    memset(flash.bytes, 0xFF, sizeof(flash.bytes));
    memset(registered, 0, sizeof(registered));
    memset(registrations, 0, sizeof(registrations));
    rpcs_sent  = 0u;
    // The slot flash adapter's RAM, as a restart leaves it.
    noah_profile_flash_store_init(noah_qmk_profile_flash_store(), noah_qmk_profile_flash_driver());
    half_known = true;
    master     = false;
    keys_busy  = false;
    noah_qmk_profile_split_transport_reset_for_test();
    noah_profile_store_runtime_init();
}

static void scan(void) {
    noah_profile_store_runtime_matrix_scan();
    scan_now++;
}

// Whether the owner has built its split reconciler: the save's copy to the
// other half is readable only then.
static bool reconciler_built(void) {
    noah_profile_candidate_v1_copy_status_t other_half;

    return noah_profile_store_runtime_other_half_transfer(&other_half);
}

static void scan_until_ready(void) {
    for (uint32_t guard = 0u; guard < BOOT_SCANS && noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING; guard++) {
        scan();
    }
}

// On an empty flash the owner validates the compiled defaults, reads both
// slots and runs the defaults: the runtime says compiled fallback. Nothing is
// written.
static void test_an_empty_flash_boots_the_compiled_defaults(void) {
    noah_profile_owner_status_t status;

    boot();
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING);
    assert(noah_profile_store_runtime_discovery_result() == NOAH_PROFILE_STORE_NO_SAVED_PROFILE);
    assert(flash.reads == 0u);
    scan_until_ready();
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK);
    assert(noah_profile_store_runtime_discovery_result() == NOAH_PROFILE_STORE_NO_SAVED_PROFILE);
    assert(noah_profile_store_runtime_saved() == NULL);
    assert(noah_profile_store_runtime_owner_status(&status) && status.owner_state == NOAH_PROFILE_OWNER_READY_COMPILED);
    assert(flash.reads > 0u && flash.erases == 0u && flash.programs == 0u);
}

// Both split RPCs are registered together, in the step in which the owner
// has built its reconciler and working copy: not at init, not in the first
// steps that validate the compiled defaults, and never again. The steps are
// taken one at a time: a grant repeats them, and one scan boots this small
// profile whole.
static void test_both_rpcs_are_registered_once_the_owner_has_built_them(void) {
    uint32_t unbuilt_steps = 0u;

    boot();
    assert(total_registrations() == 0u && !reconciler_built());
    for (uint32_t guard = 0u; guard < BOOT_SCANS && !reconciler_built(); guard++) {
        (void)noah_qmk_durable_io_store_step(NULL, false, scan_now++);
        assert(total_registrations() == (reconciler_built() ? 2u : 0u));
        unbuilt_steps += reconciler_built() ? 0u : 1u;
    }
    assert(reconciler_built() && unbuilt_steps > 0u);
    for (uint32_t later = 0u; later < BOOT_SCANS; later++) {
        scan();
    }
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK);
    for (int8_t id = 0; id < TRANSACTION_IDS; id++) {
        assert(registrations[id] == (id == PUT_PROFILE_SPLIT_SYNC || id == PUT_PROFILE_WORKING_SYNC ? 1u : 0u));
    }
}

// A request from the other half on PUT_PROFILE_SPLIT_SYNC: a metadata poll,
// or a copy's abort.
static void split_request(uint8_t kind, uint32_t generation, uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    noah_profile_split_v1_frame_t frame = {.kind = kind, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK};

    // An abort names the copy it ends; a poll names nothing.
    if (kind != NOAH_PROFILE_SPLIT_V1_METADATA) {
        frame.descriptor = (noah_profile_split_descriptor_t){
            .readable       = true,
            .has_profile    = true,
            .schema_major   = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
            .schema_minor   = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
            .generation     = generation,
            .payload_length = sizeof(compiled_blob),
        };
    }
    assert(noah_profile_split_v1_frame_encode(&frame, request, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE) == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
}

// The reply, as the split slave thread's callback writes it.
static noah_profile_split_v1_frame_t split_answer(uint8_t kind, uint32_t generation) {
    uint8_t                       request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t                       response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    noah_profile_split_v1_frame_t reply;

    split_request(kind, generation, request);
    registered[PUT_PROFILE_SPLIT_SYNC](sizeof(request), request, sizeof(response), response);
    assert(noah_profile_split_v1_reply_decode(response, sizeof(response), &reply));
    return reply;
}

// Each registered RPC answers from the owner's own endpoint: a reply that
// decodes was answered, not refused with zeros.
static void test_the_registered_rpcs_answer_from_the_owner(void) {
    uint8_t request[NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE] = {NOAH_PROFILE_WORKING_SYNC_STATUS, 1u};
    uint8_t response[NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE];

    boot();
    scan_until_ready();
    (void)split_answer(NOAH_PROFILE_SPLIT_V1_METADATA, 0u);
    noah_profile_working_sync_seal(request, sizeof(request));
    memset(response, 0, sizeof(response));
    registered[PUT_PROFILE_WORKING_SYNC](sizeof(request), request, sizeof(response), response);
    assert(response[0] == (NOAH_PROFILE_WORKING_SYNC_STATUS | 0x80u) && noah_profile_working_sync_frame_length(response, sizeof(response)) == sizeof(response));
}

// A refused registration (here the transport's second in one image, as a
// second start of the runtime would ask) is an integration error: the runtime
// says so, and its step does no more work, reads no flash and serves no edit.
static void test_a_refused_registration_is_an_integration_error(void) {
    noah_profile_owner_status_t status;
    uint32_t                    reads;

    boot();
    scan_until_ready();
    noah_profile_store_runtime_init();
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING);
    for (uint32_t guard = 0u; guard < BOOT_SCANS && noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING; guard++) {
        scan();
    }
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR);
    assert(registrations[PUT_PROFILE_SPLIT_SYNC] == 1u && registrations[PUT_PROFILE_WORKING_SYNC] == 1u);
    reads = flash.reads;
    for (uint32_t later = 0u; later < BOOT_SCANS; later++) {
        scan();
        assert(!noah_qmk_durable_io_store_step(NULL, false, scan_now));
        assert(!noah_qmk_durable_io_store_busy(NULL));
    }
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR);
    assert(flash.reads == reads && flash.erases == 0u && flash.programs == 0u);
    assert(!noah_profile_store_runtime_owner_status(&status) && noah_profile_store_runtime_saved() == NULL);
    assert(noah_profile_store_runtime_set_edit_mode(NOAH_PROFILE_EDIT_MODE_DEFAULT) == NOAH_PROFILE_WORKING_INVALID_STATE);
    assert(!noah_profile_store_runtime_usb_restart_safe());
}

// A half that cannot name itself has no owner: an integration error from init.
static void test_an_unknown_half_is_an_integration_error(void) {
    boot();
    half_known = false;
    noah_qmk_profile_split_transport_reset_for_test();
    noah_profile_store_runtime_init();
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR);
    for (uint32_t later = 0u; later < BOOT_SCANS; later++) {
        scan();
    }
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR);
    assert(total_registrations() == 0u && flash.reads == 0u);
}

// The owner's pause is this half's grant (D-F29, D-F32). A slot is taken
// here through the half's flash I/O, as a save's first step takes it; while
// the keyboard is busy its header sector cannot be erased, so the grant gives
// the step no turn. The working status and the reconciler's busy reply on the
// split slave thread then say storage paused; a quiet keyboard ends it.
static void test_the_owner_asks_the_grant_whether_storage_is_paused(void) {
    noah_profile_owner_working_status_t working;
    noah_profile_split_v1_frame_t       reply;
    noah_profile_store_io_t             io = noah_qmk_profile_flash_io();

    boot();
    scan_until_ready();
    // Unpaused, a request behind a full mailbox is answered MAILBOX_FULL.
    reply = split_answer(NOAH_PROFILE_SPLIT_V1_ABORT, 7u);
    assert(reply.kind == NOAH_PROFILE_SPLIT_V1_ACK && reply.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY && reply.busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_ADMITTED);
    reply = split_answer(NOAH_PROFILE_SPLIT_V1_ABORT, 8u);
    assert(reply.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY && reply.busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_MAILBOX_FULL);
    assert(noah_profile_store_runtime_working_status(&working) && !working.storage_paused);
    scan();
    assert(io.reset(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE));
    keys_busy = true;
    for (uint32_t typing = 0u; typing < 8u; typing++) {
        scan();
        assert(noah_profile_store_runtime_working_status(&working) && working.storage_paused);
    }
    reply = split_answer(NOAH_PROFILE_SPLIT_V1_ABORT, 9u);
    assert(reply.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY && reply.busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_ADMITTED);
    reply = split_answer(NOAH_PROFILE_SPLIT_V1_ABORT, 10u);
    assert(reply.status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY && reply.busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_STORAGE_PAUSED);
    assert(flash.erases == 0u);
    keys_busy = false;
    scan();
    assert(flash.erases == 1u);
    assert(noah_profile_store_runtime_working_status(&working) && !working.storage_paused);
}

static void write_u32(uint8_t *target, uint32_t value) {
    for (uint8_t index = 0u; index < 4u; index++) target[index] = (uint8_t)(value >> (8u * index));
}

// A host's Apply begins: the candidate declared, no bytes sent yet.
static void begin_candidate(uint16_t transaction_id) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE] = {
        NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET, NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL, NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN, (uint8_t)transaction_id, (uint8_t)(transaction_id >> 8u), NOAH_PROFILE_SCHEMA_MAJOR, 0u, 0u,
    };

    write_u32(&frame[8], sizeof(compiled_blob));
    write_u32(&frame[12], crc_of(compiled_blob, sizeof(compiled_blob)));
    write_u32(&frame[16], noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, compiled_blob, sizeof(compiled_blob)));
    write_u32(&frame[20], UINT32_C(0x12345678));
    assert(noah_profile_store_runtime_candidate_receive(frame, sizeof(frame)));
}

static noah_profile_candidate_v1_status_t candidate(void) {
    noah_profile_candidate_v1_status_t status;

    assert(noah_profile_store_runtime_candidate_status(&status));
    return status;
}

static void scan_at(uint32_t now) {
    scan_now = now;
    noah_profile_store_runtime_matrix_scan();
}

// The owner runs on the grant's clock, which leaves out the grants its step
// could not run (D-F32). A host's Apply takes a slot while keys are held, so
// its erase, and with it this half's storage, waits for a quiet keyboard; the
// Apply then times out after 15 s of the owner's clock, not of the scan's.
static void test_the_owner_runs_on_the_grants_clock(void) {
    noah_profile_owner_working_status_t working;
    uint32_t                            start = 50000u;
    uint32_t                            quiet = start + 60000u;
    uint32_t                            erases;

    boot();
    master = true;
    scan_until_ready();
    erases    = flash.erases;
    keys_busy = true;
    begin_candidate(21u);
    scan_at(start);
    assert(candidate().state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);
    // From the next grant on the step waits: the clock stops at start + 1.
    scan_at(start + 1u);
    assert(noah_profile_store_runtime_working_status(&working) && working.storage_paused);
    scan_at(start + NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS);
    scan_at(quiet - 1u);
    assert(candidate().state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING && flash.erases == erases);
    keys_busy = false;
    scan_at(quiet);
    assert(noah_profile_store_runtime_working_status(&working) && !working.storage_paused && flash.erases == erases + 1u);
    assert(candidate().state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);
    scan_at(quiet + NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS - 2u);
    assert(candidate().state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING);
    scan_at(quiet + NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS - 1u);
    assert(candidate().state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE && candidate().error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT);
    assert(noah_profile_store_runtime_state() == NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK);
    // As the master half it asked the other half (which never answers here).
    assert(rpcs_sent != 0u);
}

int main(void) {
    test_an_empty_flash_boots_the_compiled_defaults();
    test_both_rpcs_are_registered_once_the_owner_has_built_them();
    test_the_registered_rpcs_answer_from_the_owner();
    test_a_refused_registration_is_an_integration_error();
    test_an_unknown_half_is_an_integration_error();
    test_the_owner_asks_the_grant_whether_storage_is_paused();
    test_the_owner_runs_on_the_grants_clock();
    puts("profile store runtime owner host tests passed");
    return 0;
}
