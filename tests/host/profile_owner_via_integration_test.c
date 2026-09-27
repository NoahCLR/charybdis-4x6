// The live-profile owner and the logical VIA staging channel, run together.
//
// The owner tests fake the VIA layer and the VIA tests fake the owner; each
// side's fake can agree with a contract the other side does not keep. Here the
// USB half runs the real owner, the real host staging handler and the real VIA
// split sync, wired as profile_store_runtime.c wires them. The other half runs
// a real owner for the custom profile and a small VIA bank that answers the
// split RPC the way the real receiver does.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "transactions.h"
#include "users/noah/lib/compat/qmk_via_logical_profile.h"
#include "users/noah/lib/compat/qmk_via_split_sync.h"
#include "users/noah/lib/compat/qmk_via_storage_contract.h"
#include "users/noah/lib/compat/qmk_via_storage_regions.h"
#include "users/noah/lib/compat/qmk_via_sync_metadata.h"
#include "users/noah/lib/compat/qmk_via_sync_state.h"
#include "users/noah/lib/profile/protocol/profile_candidate_v1.h"
#include "users/noah/lib/profile/protocol/profile_wire_v1.h"
#include "users/noah/lib/profile/runtime/profile_action_placement_v1.h"
#include "users/noah/lib/profile/runtime/profile_owner.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/storage/profile_store_runtime_hooks.h"

enum {
    CONFIG_SIZE = 2,
    KEYMAP_SIZE = 3,
    MACRO_SIZE  = 2,
};

static const uint8_t compiled_blob[] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};

typedef struct {
    uint8_t bytes[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
} memory_t;

typedef struct {
    noah_profile_owner_t *peer;
} split_link_t;

// ── The other half's VIA bank ────────────────────────────────────────────────

typedef struct {
    uint8_t  config[CONFIG_SIZE];
    uint8_t  keymap[KEYMAP_SIZE];
    uint8_t  macro[MACRO_SIZE];
    uint32_t generation;
    bool     dirty;
    bool     session;
    bool     staging;
    bool     staged;
    uint32_t stage_generation;
    uint32_t stage_digest;
    uint16_t aborts;
} peer_bank_t;

static noah_profile_owner_t  usb;
static noah_profile_owner_t  other;
static memory_t              usb_memory;
static memory_t              other_memory;
static split_link_t          usb_link   = {.peer = &other};
static split_link_t          other_link = {.peer = &usb};
static peer_bank_t           peer;
static bool                  link_up;
static uint32_t              fake_now;
static uint32_t              user_eeconfig_word;
static uint8_t               local_config[CONFIG_SIZE];
static uint8_t               local_keymap[KEYMAP_SIZE];
static uint8_t               local_macro[MACRO_SIZE];
static const uint8_t         target_keymap[KEYMAP_SIZE] = {0x91u, 0x92u, 0x93u};
static noah_profile_owner_t *admitting_owner;

static uint32_t digest_parts(const uint8_t *config, const uint8_t *keymap, const uint8_t *macro) {
    uint32_t hash = UINT32_C(0x811C9DC5);

    for (uint8_t index = 0u; index < CONFIG_SIZE; index++)
        hash = (hash ^ config[index]) * UINT32_C(16777619);
    for (uint8_t index = 0u; index < KEYMAP_SIZE; index++)
        hash = (hash ^ keymap[index]) * UINT32_C(16777619);
    for (uint8_t index = 0u; index < MACRO_SIZE; index++)
        hash = (hash ^ macro[index]) * UINT32_C(16777619);
    return hash;
}

static uint32_t peer_digest(void) {
    return digest_parts(peer.config, peer.keymap, peer.macro);
}

static uint32_t local_digest(void) {
    return digest_parts(local_config, local_keymap, local_macro);
}

static uint32_t target_digest(void) {
    return digest_parts(local_config, target_keymap, local_macro);
}

static uint8_t *peer_region(noah_qmk_via_sync_region_t region) {
    return region == NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG ? peer.config : region == NOAH_QMK_VIA_SYNC_REGION_KEYMAP ? peer.keymap : region == NOAH_QMK_VIA_SYNC_REGION_MACRO ? peer.macro : NULL;
}

static uint8_t *local_region(noah_qmk_via_sync_region_t region) {
    return region == NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG ? local_config : region == NOAH_QMK_VIA_SYNC_REGION_KEYMAP ? local_keymap : region == NOAH_QMK_VIA_SYNC_REGION_MACRO ? local_macro : NULL;
}

static noah_qmk_via_sync_frame_t peer_reply(noah_qmk_via_sync_message_kind_t kind, noah_qmk_via_sync_status_t status, const noah_qmk_via_sync_frame_t *request) {
    return (noah_qmk_via_sync_frame_t){.kind = kind, .status = status, .generation = request->generation, .digest = request->digest};
}

// Mirrors the receiver's answers in qmk_via_split_sync.c, including the ones
// the fix depends on: an ABORT for a staging it does not hold, an ACCEPT for
// one it no longer holds, and a dirty bank until a complete copy lands.
static noah_qmk_via_sync_frame_t peer_answer(const noah_qmk_via_sync_frame_t *request) {
    noah_qmk_via_sync_frame_t response;
    uint8_t                  *bytes = peer_region(request->region);

    switch (request->kind) {
        case NOAH_QMK_VIA_SYNC_MESSAGE_METADATA:
            return (noah_qmk_via_sync_frame_t){.kind = NOAH_QMK_VIA_SYNC_MESSAGE_METADATA, .status = peer.dirty ? NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED : NOAH_QMK_VIA_SYNC_STATUS_OK, .generation = peer.generation, .digest = peer_digest()};
        case NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN:
            peer.session = peer.dirty = true;
            peer.staging = peer.staged = false;
            return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request);
        case NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_BEGIN:
            peer.staging = peer.dirty = true;
            peer.session = peer.staged = false;
            peer.stage_generation      = request->generation;
            peer.stage_digest          = request->digest;
            return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request);
        case NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK:
            if ((!peer.session && !peer.staging) || !bytes) {
                return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, request);
            }
            memcpy(&bytes[request->offset], request->payload, request->payload_length);
            response               = peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request);
            response.region        = request->region;
            response.offset        = request->offset + request->payload_length;
            response.region_length = request->region_length;
            return response;
        case NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT:
            if (!peer.session || peer_digest() != request->digest) {
                return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, request);
            }
            peer.session    = false;
            peer.dirty      = false;
            peer.generation = request->generation;
            return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request);
        case NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_VERIFY:
            if (!peer.staging || request->generation != peer.stage_generation || request->digest != peer.stage_digest || peer_digest() != request->digest) {
                return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, request);
            }
            peer.staged = true;
            return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request);
        case NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ACCEPT:
            if (peer.staged && request->generation == peer.stage_generation && request->digest == peer.stage_digest) {
                peer.staging = peer.staged = peer.dirty = false;
                peer.generation                         = request->generation;
                return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request);
            }
            if (!peer.dirty && peer.generation == request->generation && peer_digest() == request->digest) {
                return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request);
            }
            return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, request);
        case NOAH_QMK_VIA_SYNC_MESSAGE_LOGICAL_STAGE_ABORT:
            peer.aborts++;
            if (!peer.staging || request->generation != peer.stage_generation || request->digest != peer.stage_digest) {
                return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ERROR, NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED, request);
            }
            // The staged bytes stay written and the bank dirty, so ordinary
            // reconciliation copies the USB half's old bank back.
            peer.staging = peer.staged = false;
            return peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_ACK, NOAH_QMK_VIA_SYNC_STATUS_OK, request);
        case NOAH_QMK_VIA_SYNC_MESSAGE_PULL_CHUNK: {
            uint16_t remaining = request->region_length - request->offset;

            assert(bytes != NULL);
            response                = peer_reply(NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK, NOAH_QMK_VIA_SYNC_STATUS_OK, request);
            response.region         = request->region;
            response.offset         = request->offset;
            response.region_length  = request->region_length;
            response.payload_length = remaining < NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX ? (uint8_t)remaining : NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX;
            memcpy(response.payload, &bytes[request->offset], response.payload_length);
            return response;
        }
        default:
            assert(false);
            return (noah_qmk_via_sync_frame_t){0};
    }
}

// ── QMK and VIA storage seams for the USB half ──────────────────────────────

bool is_keyboard_master(void) {
    return true;
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
    return true;
}

bool noah_via_macro_defaults_reseed_for_recovery(void) {
    return true;
}

void via_macro_provider_invalidate_all(void) {}

void noah_rgb_runtime_invalidate_layer_maps(void) {}

bool via_eeprom_is_valid(void) {
    return local_config[0] != 0u;
}

void via_eeprom_set_valid(bool valid) {
    local_config[0] = valid ? 1u : 0u;
}

void eeconfig_init_via(void) {
    assert(false);
}

uint16_t noah_qmk_via_storage_region_size(noah_qmk_via_sync_region_t region) {
    return region == NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG ? CONFIG_SIZE : region == NOAH_QMK_VIA_SYNC_REGION_KEYMAP ? KEYMAP_SIZE : region == NOAH_QMK_VIA_SYNC_REGION_MACRO ? MACRO_SIZE : 0u;
}

bool noah_qmk_via_storage_region_read(noah_qmk_via_sync_region_t region, uint16_t offset, uint8_t *data, uint8_t length) {
    uint8_t *source = local_region(region);

    if (!source || !data || offset > noah_qmk_via_storage_region_size(region) || length > noah_qmk_via_storage_region_size(region) - offset) return false;
    memcpy(data, &source[offset], length);
    return true;
}

bool noah_qmk_via_storage_region_write(noah_qmk_via_sync_region_t region, uint16_t offset, const uint8_t *data, uint8_t length) {
    uint8_t *destination = local_region(region);

    if (!destination || !data || offset > noah_qmk_via_storage_region_size(region) || length > noah_qmk_via_storage_region_size(region) - offset) return false;
    memcpy(&destination[offset], data, length);
    return true;
}

void noah_qmk_via_storage_digest_init(noah_qmk_via_storage_digest_cursor_t *cursor) {
    *cursor = (noah_qmk_via_storage_digest_cursor_t){0};
}

bool noah_qmk_via_storage_digest_step(noah_qmk_via_storage_digest_cursor_t *cursor, uint8_t byte_budget, uint32_t *out_digest) {
    (void)byte_budget;
    cursor->complete = true;
    *out_digest      = local_digest();
    return true;
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    assert(transaction_id == PUT_VIA_KEYMAP_SYNC);
    (void)callback;
}

bool transaction_rpc_exec(int8_t transaction_id, uint8_t request_size, const void *request_data, uint8_t response_size, void *response_data) {
    noah_qmk_via_sync_frame_t request;
    noah_qmk_via_sync_frame_t response;

    assert(transaction_id == PUT_VIA_KEYMAP_SYNC);
    assert(noah_qmk_via_sync_frame_decode(request_data, request_size, &request));
    if (!link_up) return false;
    response = peer_answer(&request);
    assert(response_size == NOAH_QMK_VIA_SYNC_FRAME_SIZE);
    assert(noah_qmk_via_sync_frame_encode(&response, response_data));
    return true;
}

// profile_store_runtime.c's glue for the host staging handler.
bool noah_profile_store_runtime_logical_via_admit(uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    return admitting_owner && noah_profile_owner_logical_via_admit(admitting_owner, transaction_id, generation, digest);
}

void noah_profile_store_runtime_logical_via_progress(void) {
    if (admitting_owner) noah_profile_owner_logical_via_progress(admitting_owner);
}

// ── The owner's environment ─────────────────────────────────────────────────

static uint32_t crc_of(const uint8_t *bytes, size_t length) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length));
}

static uint32_t fnv_of(const uint8_t *bytes, size_t length) {
    return noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, bytes, length);
}

static bool compiled_read(void *context, size_t offset, uint8_t *target, size_t length) {
    (void)context;
    if (!target || length == 0u || offset > sizeof(compiled_blob) || length > sizeof(compiled_blob) - offset) return false;
    memcpy(target, &compiled_blob[offset], length);
    return true;
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_open(noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_error_t *error) {
    (void)error;
    *profile = (noah_profile_compiled_v1_t){.metadata = {.crc32 = crc_of(compiled_blob, sizeof(compiled_blob)), .digest = fnv_of(compiled_blob, sizeof(compiled_blob)), .action_abi_digest = UINT32_C(0x12345678), .byte_length = sizeof(compiled_blob)}};
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
    *compatibility                      = noah_profile_validator_v1_default_compatibility(profile->metadata.action_abi_digest);
    compatibility->required_domain_mask = 0u;
    return true;
}

void noah_effective_key_behavior_runtime_init(noah_effective_key_behavior_runtime_t *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->initialized = true;
}

bool noah_effective_key_behavior_runtime_install(noah_effective_key_behavior_runtime_t *runtime) {
    return runtime && runtime->initialized;
}

void noah_effective_key_behavior_runtime_uninstall(noah_effective_key_behavior_runtime_t *runtime) {
    (void)runtime;
}

void noah_effective_key_behavior_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view) {
    (void)context;
    (void)publication_count;
    (void)previous;
    (void)active;
    (void)callback_view;
}

void noah_effective_rgb_runtime_init(noah_effective_rgb_runtime_t *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->initialized = true;
}

bool noah_effective_rgb_runtime_install(noah_effective_rgb_runtime_t *runtime) {
    return runtime && runtime->initialized;
}

void noah_effective_rgb_runtime_uninstall(noah_effective_rgb_runtime_t *runtime) {
    (void)runtime;
}

void noah_effective_rgb_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view) {
    (void)context;
    (void)publication_count;
    (void)previous;
    (void)active;
    (void)callback_view;
}

void noah_profile_activation_policy_init(noah_profile_activation_policy_t *policy, noah_profile_activation_peer_observer_fn observer, void *observer_context) {
    memset(policy, 0, sizeof(*policy));
    policy->peer_observer = observer;
    policy->peer_context  = observer_context;
    policy->initialized   = true;
}

uint32_t noah_profile_activation_policy_safe_boundary(void *context) {
    noah_profile_activation_policy_t *policy     = context;
    uint8_t                           unresolved = 1u;
    return policy && policy->initialized && policy->peer_observer && policy->peer_observer(policy->peer_context, &unresolved) && unresolved == 0u ? 0u : NOAH_PROFILE_ACTIVATION_REASON_PEER;
}

static bool memory_read(void *context, uint16_t address, uint8_t *target, uint16_t length) {
    memory_t *memory = context;
    if (!target || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) return false;
    memcpy(target, &memory->bytes[address], length);
    return true;
}

static bool memory_write(void *context, uint16_t address, const uint8_t *source, uint16_t length) {
    memory_t *memory = context;
    if (!source || length == 0u || (uint32_t)address + length > sizeof(memory->bytes)) return false;
    memcpy(&memory->bytes[address], source, length);
    return true;
}

static bool split_exchange(void *context, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    split_link_t                    *link = context;
    noah_profile_split_reconciler_t *reconciler;

    if (!link_up || !link || !link->peer || !(reconciler = noah_profile_owner_split_reconciler(link->peer))) return false;
    return noah_profile_split_reconciler_receive(reconciler, request, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, response, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
}

// The USB half's owner drives the real VIA layer, as profile_store_runtime.c
// wires it.
static bool usb_via_ready(void *context, uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    (void)context;
    return noah_qmk_via_logical_staged(transaction_id, generation, digest);
}

static bool usb_via_accept(void *context, uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    (void)context;
    return noah_qmk_via_logical_profile_accept(transaction_id, generation, digest);
}

static bool usb_via_abort(void *context, uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    (void)context;
    return noah_qmk_via_logical_cancel(transaction_id, generation, digest);
}

static bool usb_via_converged(void *context, uint32_t generation, uint32_t digest) {
    (void)context;
    return noah_qmk_via_logical_converged(generation, digest);
}

static bool usb_via_boot_recover(void *context, uint32_t generation, uint32_t digest) {
    (void)context;
    return noah_qmk_via_logical_boot_recover(generation, digest);
}

static void usb_via_boot_release(void *context) {
    (void)context;
    noah_qmk_via_logical_boot_release();
}

static const noah_profile_logical_via_ops_t usb_via_ops = {
    .ready        = usb_via_ready,
    .accept       = usb_via_accept,
    .abort        = usb_via_abort,
    .converged    = usb_via_converged,
    .boot_recover = usb_via_boot_recover,
    .boot_release = usb_via_boot_release,
};

// The other half's owner sees its own VIA bank.
static bool other_via_never(void *context, uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    (void)context;
    (void)transaction_id;
    (void)generation;
    (void)digest;
    return false;
}

static bool other_via_converged(void *context, uint32_t generation, uint32_t digest) {
    (void)context;
    return !peer.dirty && peer.generation == generation && peer_digest() == digest;
}

static bool other_via_boot_recover(void *context, uint32_t generation, uint32_t digest) {
    (void)context;
    (void)generation;
    (void)digest;
    return true;
}

static void other_via_boot_release(void *context) {
    (void)context;
}

static const noah_profile_logical_via_ops_t other_via_ops = {
    .ready        = other_via_never,
    .accept       = other_via_never,
    .abort        = other_via_never,
    .converged    = other_via_converged,
    .boot_recover = other_via_boot_recover,
    .boot_release = other_via_boot_release,
};

// ── Frames and scheduling ───────────────────────────────────────────────────

static void write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static void candidate_frame(uint8_t frame[32], uint8_t value, uint16_t transaction_id) {
    memset(frame, 0, 32u);
    frame[0] = value == NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT ? NOAH_PROFILE_CANDIDATE_V1_COMMAND_SAVE : NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = value;
    write_u16(&frame[3], transaction_id);
    if (value == NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN) {
        frame[5] = 1u;
        write_u16(&frame[9], sizeof(compiled_blob));
        write_u32(&frame[11], crc_of(compiled_blob, sizeof(compiled_blob)));
        write_u32(&frame[15], fnv_of(compiled_blob, sizeof(compiled_blob)));
        write_u32(&frame[19], UINT32_C(0x12345678));
        frame[23] = NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL;
        write_u32(&frame[24], 6u);
        write_u32(&frame[28], target_digest());
    } else if (value == NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK) {
        frame[7] = sizeof(compiled_blob);
        memcpy(&frame[8], compiled_blob, sizeof(compiled_blob));
    }
}

// Sends one host staging frame through the real handler; returns admission.
static uint8_t host_via(uint8_t value, uint16_t transaction_id, noah_qmk_via_sync_region_t region, uint16_t offset, const uint8_t *bytes, uint8_t length) {
    uint8_t frame[32] = {0};

    frame[0] = NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = value;
    write_u16(&frame[3], transaction_id);
    if (value == NOAH_QMK_VIA_LOGICAL_VALUE_CHUNK) {
        frame[5] = (uint8_t)region;
        write_u16(&frame[6], offset);
        write_u16(&frame[8], noah_qmk_via_storage_region_size(region));
        frame[10] = length;
        memcpy(&frame[11], bytes, length);
        write_u32(&frame[23], 6u);
        write_u32(&frame[27], target_digest());
    } else {
        write_u32(&frame[5], 6u);
        write_u32(&frame[9], target_digest());
    }
    assert(noah_qmk_via_logical_profile_handle(frame, sizeof(frame)));
    return frame[5];
}

static noah_qmk_via_logical_status_t via_status(void) {
    noah_qmk_via_logical_status_t status;
    assert(noah_qmk_via_logical_status(&status));
    return status;
}

static void tick(uint32_t *now) {
    fake_now = *now;
    (void)noah_qmk_via_split_sync_matrix_scan_step();
    (void)noah_profile_owner_scan(&other, false, *now);
    (void)noah_profile_owner_scan(&usb, true, *now);
    *now += NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
}

static void ticks(uint32_t *now, uint32_t count) {
    for (uint32_t index = 0u; index < count; index++)
        tick(now);
}

static void host_candidate(uint8_t value, uint16_t transaction_id, uint32_t *now) {
    uint8_t frame[32];

    candidate_frame(frame, value, transaction_id);
    assert(noah_profile_owner_receive(&usb, frame, sizeof(frame)));
    for (uint32_t guard = 0u; guard < 64u && usb.host_transaction.mailbox.pending; guard++)
        tick(now);
    assert(!usb.host_transaction.mailbox.pending);
}

// A staging frame the handler queued, carried to the peer.
static void host_staged(uint8_t value, uint16_t transaction_id, noah_qmk_via_sync_region_t region, const uint8_t *bytes, uint8_t length, uint32_t *now) {
    assert(host_via(value, transaction_id, region, 0u, bytes, length) == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED);
    for (uint32_t guard = 0u; guard < 64u && via_status().pending; guard++)
        tick(now);
    assert(!via_status().pending);
}

static void boot_pair(uint32_t *now) {
    memset(&usb_memory, 0xff, sizeof(usb_memory));
    memset(&other_memory, 0xff, sizeof(other_memory));
    local_config[0] = 1u;
    local_config[1] = 2u;
    local_keymap[0] = 3u;
    local_keymap[1] = 4u;
    local_keymap[2] = 5u;
    local_macro[0]  = 6u;
    local_macro[1]  = 7u;
    peer            = (peer_bank_t){.generation = 5u};
    memcpy(peer.config, local_config, sizeof(local_config));
    memcpy(peer.keymap, local_keymap, sizeof(local_keymap));
    memcpy(peer.macro, local_macro, sizeof(local_macro));
    user_eeconfig_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 5u});
    link_up            = true;
    admitting_owner    = &usb;
    fake_now           = *now;
    noah_qmk_via_split_sync_init();
    assert(noah_profile_owner_init(&usb, &(noah_profile_owner_config_t){.store_io = {.read = memory_read, .write = memory_write, .context = &usb_memory}, .split_exchange = split_exchange, .split_transport_context = &usb_link, .origin_half = 0u, .peer_required = true, .logical_via = &usb_via_ops}));
    assert(noah_profile_owner_init(&other, &(noah_profile_owner_config_t){.store_io = {.read = memory_read, .write = memory_write, .context = &other_memory}, .split_exchange = split_exchange, .split_transport_context = &other_link, .origin_half = 1u, .peer_required = true, .logical_via = &other_via_ops}));
    for (uint32_t guard = 0u; guard < 4096u && (usb.state != NOAH_PROFILE_OWNER_READY_COMPILED || other.state != NOAH_PROFILE_OWNER_READY_COMPILED); guard++)
        tick(now);
    assert(usb.state == NOAH_PROFILE_OWNER_READY_COMPILED && other.state == NOAH_PROFILE_OWNER_READY_COMPILED);
    assert(noah_qmk_via_logical_mirror_allowed());
}

// Uploads and validates the custom candidate, then stages the target keymap
// on the peer: BEGIN, one chunk, and (when `verify`) VERIFY.
static void stage(uint16_t transaction_id, bool verify, uint32_t *now) {
    host_candidate(NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN, transaction_id, now);
    host_candidate(NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK, transaction_id, now);
    host_candidate(NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE, transaction_id, now);
    for (uint32_t guard = 0u; guard < 64u && usb.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED; guard++)
        tick(now);
    assert(usb.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    host_staged(NOAH_QMK_VIA_LOGICAL_VALUE_BEGIN, transaction_id, NOAH_QMK_VIA_SYNC_REGION_NONE, NULL, 0u, now);
    host_staged(NOAH_QMK_VIA_LOGICAL_VALUE_CHUNK, transaction_id, NOAH_QMK_VIA_SYNC_REGION_KEYMAP, target_keymap, KEYMAP_SIZE, now);
    if (verify) {
        host_staged(NOAH_QMK_VIA_LOGICAL_VALUE_VERIFY, transaction_id, NOAH_QMK_VIA_SYNC_REGION_NONE, NULL, 0u, now);
        assert(via_status().state == NOAH_QMK_VIA_LOGICAL_STAGED);
    } else {
        assert(via_status().state == NOAH_QMK_VIA_LOGICAL_STAGING);
    }
    assert(memcmp(peer.keymap, target_keymap, KEYMAP_SIZE) == 0);
}

// Both halves hold the old bank again, reconciled, and nothing is staged.
static void check_old_bank_restored(uint32_t *now) {
    for (uint32_t guard = 0u; guard < 4096u && (peer.dirty || memcmp(peer.keymap, local_keymap, KEYMAP_SIZE) != 0); guard++)
        tick(now);
    assert(!peer.dirty && !peer.staging);
    assert(memcmp(peer.keymap, local_keymap, KEYMAP_SIZE) == 0 && local_keymap[0] == 3u);
    assert(peer.generation == 5u && noah_qmk_via_sync_state_snapshot().metadata.generation == 5u);
    assert(via_status().state == NOAH_QMK_VIA_LOGICAL_ABORTED && !via_status().pending);
    assert(noah_qmk_via_logical_mirror_allowed());
    assert(usb.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(noah_profile_candidate_store_backend_admission_owner(&usb.staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    assert(usb.store.committed.slot == NOAH_PROFILE_SLOT_NONE);
}

// Commits a staged candidate and, when `roll_forward`, writes the target into
// the USB half's bank as the host does after ACCEPT. Ends activated.
static void commit_staged(uint16_t transaction_id, bool roll_forward, uint32_t *now) {
    host_candidate(NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, transaction_id, now);
    for (uint32_t guard = 0u; guard < 8192u && via_status().state != NOAH_QMK_VIA_LOGICAL_ACCEPTED; guard++)
        tick(now);
    assert(via_status().state == NOAH_QMK_VIA_LOGICAL_ACCEPTED);
    assert(peer.generation == 6u && !peer.dirty);
    if (roll_forward) {
        memcpy(local_keymap, target_keymap, KEYMAP_SIZE);
        noah_qmk_via_split_sync_note_mutation(NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR);
    }
    for (uint32_t guard = 0u; guard < 16384u && (usb.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE || usb.state != NOAH_PROFILE_OWNER_READY_VALIDATED); guard++)
        tick(now);
    assert(usb.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE && usb.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert(memcmp(local_keymap, target_keymap, KEYMAP_SIZE) == 0);
    assert(noah_qmk_via_sync_state_snapshot().metadata.generation == 6u && !noah_qmk_via_sync_state_snapshot().metadata.dirty);
    assert(noah_profile_owner_output_ready(&usb));
}

// ── Scenarios ───────────────────────────────────────────────────────────────

// The host sent COMMIT and then lost sight of the keyboard before it saw
// CONVERGING_PEER, so it tries to cancel. The commit marker is already
// durable: neither channel may cancel, and the keyboard finishes by itself.
static void test_a_decision_the_host_did_not_see_rolls_forward(void) {
    uint32_t now = 250000u;
    uint8_t  frame[32];

    boot_pair(&now);
    stage(301u, true, &now);
    candidate_frame(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT, 301u);
    assert(noah_profile_owner_receive(&usb, frame, sizeof(frame)));
    for (uint32_t guard = 0u; guard < 8192u && usb.store.committed.slot == NOAH_PROFILE_SLOT_NONE; guard++)
        tick(&now);
    assert(usb.store.committed.slot != NOAH_PROFILE_SLOT_NONE);

    assert(host_via(NOAH_QMK_VIA_LOGICAL_VALUE_ABORT, 301u, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, NULL, 0u) == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_UNSUPPORTED);
    host_candidate(NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 301u, &now);
    assert(usb.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE);
    assert(peer.aborts == 0u);

    // The host is gone: nothing rolls the USB half forward, so the VIA layer
    // releases its hold and pulls the peer's accepted copy.
    for (uint32_t guard = 0u; guard < 8192u && via_status().state != NOAH_QMK_VIA_LOGICAL_ACCEPTED; guard++)
        tick(&now);
    assert(via_status().state == NOAH_QMK_VIA_LOGICAL_ACCEPTED && peer.generation == 6u);
    for (uint32_t guard = 0u; guard < 16384u && usb.state != NOAH_PROFILE_OWNER_READY_VALIDATED; guard++)
        tick(&now);
    assert(usb.state == NOAH_PROFILE_OWNER_READY_VALIDATED && usb.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(memcmp(local_keymap, target_keymap, KEYMAP_SIZE) == 0);
    assert(noah_qmk_via_sync_state_snapshot().metadata.generation == 6u);
    assert(peer.aborts == 0u);
}

// Before the decision the host's cancel ends both stores, once, however
// often it is repeated; the next Apply then runs normally.
static void test_a_cancel_before_the_decision_releases_both_stores(void) {
    uint32_t now = 250000u;

    boot_pair(&now);
    stage(302u, true, &now);
    host_candidate(NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 302u, &now);
    check_old_bank_restored(&now);
    assert(peer.aborts == 1u);
    host_candidate(NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 302u, &now);
    ticks(&now, 16u);
    assert(peer.aborts == 1u && via_status().state == NOAH_QMK_VIA_LOGICAL_ABORTED);
    assert(host_via(NOAH_QMK_VIA_LOGICAL_VALUE_BEGIN, 302u, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, NULL, 0u) == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_UNSUPPORTED);

    stage(303u, true, &now);
    commit_staged(303u, true, &now);
}

// The host went away while staging, before COMMIT. The candidate's lease
// ends the staging with it, whether VERIFY had run or not.
static void test_an_abandoned_staging_expires_with_its_candidate(void) {
    for (uint8_t verified = 0u; verified < 2u; verified++) {
        uint32_t now = 250000u;

        boot_pair(&now);
        stage(304u, verified != 0u, &now);
        now = usb.host_last_activity_at + NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS;
        tick(&now);
        assert(usb.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
        assert(usb.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT);
        check_old_bank_restored(&now);
        stage(305u, true, &now);
        commit_staged(305u, true, &now);
    }
}

// A staging that keeps making progress outlives the 15-second lease; one
// that stops does not, however often the host polls status.
static void test_staging_progress_is_the_candidates_lease(void) {
    uint32_t      now  = 250000u;
    const uint8_t byte = target_keymap[0];

    boot_pair(&now);
    stage(306u, false, &now);
    for (uint8_t step = 0u; step < 8u; step++) {
        uint32_t resume = now + NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS / 3u;

        while (now < resume) {
            (void)via_status();
            tick(&now);
        }
        host_staged(NOAH_QMK_VIA_LOGICAL_VALUE_CHUNK, 306u, NOAH_QMK_VIA_SYNC_REGION_KEYMAP, &byte, 1u, &now);
        assert(usb.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    }
    host_staged(NOAH_QMK_VIA_LOGICAL_VALUE_VERIFY, 306u, NOAH_QMK_VIA_SYNC_REGION_NONE, NULL, 0u, &now);
    commit_staged(306u, true, &now);

    stage(307u, true, &now);
    for (uint32_t polls = 0u; usb.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED; polls++) {
        assert(polls < (NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS / NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) + 4u);
        (void)via_status();
        tick(&now);
    }
    assert(usb.host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT);
    for (uint32_t guard = 0u; guard < 4096u && via_status().state != NOAH_QMK_VIA_LOGICAL_ABORTED; guard++)
        tick(&now);
    assert(via_status().state == NOAH_QMK_VIA_LOGICAL_ABORTED);
}

// The link to the other half is down when the lease ends. The candidate is
// released at once; the peer's copy is cancelled, and its bytes restored,
// when the link returns, and only then does the next Apply stage.
static void test_cleanup_waits_for_an_absent_peer_and_resumes(void) {
    uint32_t now = 250000u;

    boot_pair(&now);
    stage(308u, true, &now);
    link_up = false;
    now     = usb.host_last_activity_at + NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS;
    ticks(&now, 64u);
    assert(usb.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE);
    assert(via_status().state == NOAH_QMK_VIA_LOGICAL_STAGED && via_status().pending);
    assert(!noah_qmk_via_logical_mirror_allowed());
    assert(peer.staged && memcmp(peer.keymap, target_keymap, KEYMAP_SIZE) == 0);

    host_candidate(NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN, 309u, &now);
    assert(host_via(NOAH_QMK_VIA_LOGICAL_VALUE_BEGIN, 309u, NOAH_QMK_VIA_SYNC_REGION_NONE, 0u, NULL, 0u) == NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY);
    host_candidate(NOAH_PROFILE_CANDIDATE_V1_VALUE_ABORT, 309u, &now);

    link_up = true;
    check_old_bank_restored(&now);
    assert(peer.aborts == 1u);
    stage(310u, true, &now);
    commit_staged(310u, true, &now);
}

int main(void) {
    test_a_decision_the_host_did_not_see_rolls_forward();
    test_a_cancel_before_the_decision_releases_both_stores();
    test_an_abandoned_staging_expires_with_its_candidate();
    test_staging_progress_is_the_candidates_lease();
    test_cleanup_waits_for_an_absent_peer_and_resumes();
    puts("profile owner and logical VIA integration tests passed");
    return 0;
}
