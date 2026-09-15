// ──────────────────────────────────────────────────────────────────────────
// Scan-Owned Live-Profile Split Reconciler
// ──────────────────────────────────────────────────────────────────────────

#include "profile_split_reconciler.h"

#include <limits.h>
#include <string.h>

#ifdef VIA_ENABLE

static noah_profile_split_descriptor_t unreadable_descriptor(void);
static void                            invalidate_peer(noah_profile_split_reconciler_t *reconciler);
static void                            cache_response(noah_profile_split_reconciler_t *reconciler, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], const noah_profile_split_v1_frame_t *response);

static bool descriptor_equal(const noah_profile_split_descriptor_t *left, const noah_profile_split_descriptor_t *right) {
    return left && right && left->generation == right->generation && left->payload_crc32 == right->payload_crc32 && left->payload_digest == right->payload_digest && left->compiled_default_digest == right->compiled_default_digest && left->action_abi_digest == right->action_abi_digest && left->payload_length == right->payload_length && left->schema_major == right->schema_major && left->schema_minor == right->schema_minor && left->domain_mask == right->domain_mask && left->profile_flags == right->profile_flags && left->origin_half == right->origin_half && left->readable == right->readable && left->has_profile == right->has_profile && left->logical == right->logical;
}

static bool descriptor_correlation_matches(const noah_profile_split_descriptor_t *descriptor, uint32_t generation, uint32_t payload_digest, uint16_t payload_length) {
    return descriptor && descriptor->readable && descriptor->has_profile && descriptor->generation == generation && descriptor->payload_digest == payload_digest && descriptor->payload_length == payload_length;
}

static void clear_provisional_peer(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor) {
    if (reconciler && reconciler->provisional_peer_descriptor_known && (!descriptor || descriptor_equal(&reconciler->provisional_peer_descriptor, descriptor))) {
        reconciler->provisional_peer_descriptor       = unreadable_descriptor();
        reconciler->provisional_peer_descriptor_known = false;
    }
}

static void observe_provisional_peer(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor) {
    if (reconciler && noah_profile_split_descriptor_valid(descriptor) && descriptor->readable && descriptor->has_profile) {
        reconciler->provisional_peer_descriptor       = *descriptor;
        reconciler->provisional_peer_descriptor_known = true;
    }
}

static void clear_prepared_push(noah_profile_split_reconciler_t *reconciler) {
    if (!reconciler) {
        return;
    }
    reconciler->prepared_source_context = NULL;
    reconciler->prepared_source_read    = NULL;
    reconciler->prepared_push_active    = false;
    reconciler->prepared_remote_started = false;
    reconciler->prepared_commit_authorized = false;
    reconciler->prepared_cancel_refused = false;
    reconciler->prepared_logical        = false;
    reconciler->prepared_via_generation = 0u;
    reconciler->prepared_via_digest     = 0u;
}

static noah_profile_split_reconciler_state_t prepared_payload_complete_state(const noah_profile_split_reconciler_t *reconciler) {
    return reconciler && reconciler->prepared_push_active ? NOAH_PROFILE_SPLIT_RECONCILER_PUSH_DURABLE : NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT;
}

static bool deadline_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

static noah_profile_split_descriptor_t unreadable_descriptor(void) {
    noah_profile_split_descriptor_t descriptor = {0};
    return descriptor;
}

static bool refresh_local(noah_profile_split_reconciler_t *reconciler) {
    noah_profile_split_descriptor_t descriptor = unreadable_descriptor();
    bool                            readable;

    if (!reconciler) {
        return false;
    }
    readable = reconciler->config.local_descriptor && reconciler->config.local_descriptor(reconciler->config.local_context, &descriptor) && noah_profile_split_descriptor_valid(&descriptor);
    if (!readable) {
        descriptor = unreadable_descriptor();
    }
    reconciler->local_descriptor = descriptor;
    return readable;
}

static void refresh_metadata_response(noah_profile_split_reconciler_t *reconciler) {
    noah_profile_split_v1_frame_t metadata = {
        .kind       = NOAH_PROFILE_SPLIT_V1_METADATA,
        .status     = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .descriptor = reconciler->local_descriptor,
    };

    noah_runtime_publication_begin(&reconciler->metadata_sequence);
    reconciler->metadata_response_valid = false;
    reconciler->metadata_response_valid = noah_profile_split_v1_frame_encode(&metadata, reconciler->metadata_response_wire);
    noah_runtime_publication_end(&reconciler->metadata_sequence);
}

static bool mailbox_pending(const noah_profile_split_reconciler_t *reconciler) {
    return reconciler && noah_runtime_publication_observe(&reconciler->mailbox_sequence) != reconciler->mailbox_consumed_sequence;
}

static bool publish_mailbox(noah_profile_split_reconciler_t *reconciler, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    if (mailbox_pending(reconciler)) {
        return false;
    }
    noah_runtime_publication_begin(&reconciler->mailbox_sequence);
    memcpy(reconciler->mailbox_wire, request, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    noah_runtime_publication_end(&reconciler->mailbox_sequence);
    return true;
}

static bool consume_mailbox(noah_profile_split_reconciler_t *reconciler, uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    for (uint8_t attempt = 0u; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t observed = noah_runtime_publication_observe(&reconciler->mailbox_sequence);

        if (observed == reconciler->mailbox_consumed_sequence || noah_runtime_publication_in_flight(observed)) {
            continue;
        }
        memcpy(request, reconciler->mailbox_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
        if (noah_runtime_publication_settled(&reconciler->mailbox_sequence, observed)) {
            reconciler->mailbox_consumed_sequence = observed;
            return true;
        }
    }
    return false;
}

static bool read_metadata_response(const noah_profile_split_reconciler_t *reconciler, uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    for (uint8_t attempt = 0u; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t observed = noah_runtime_publication_observe(&reconciler->metadata_sequence);
        bool    valid;

        if (noah_runtime_publication_in_flight(observed)) {
            continue;
        }
        valid = reconciler->metadata_response_valid;
        if (valid) {
            memcpy(response, reconciler->metadata_response_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
        }
        if (noah_runtime_publication_settled(&reconciler->metadata_sequence, observed)) {
            return valid;
        }
    }
    return false;
}

static bool transfer_pending(const noah_profile_split_reconciler_t *reconciler) {
    if (!reconciler) {
        return false;
    }
    return mailbox_pending(reconciler) || reconciler->prepared_push_active || reconciler->transfer_owner != NOAH_PROFILE_SPLIT_TRANSFER_NONE || (reconciler->state >= NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BIND && reconciler->state <= NOAH_PROFILE_SPLIT_RECONCILER_VERIFY);
}

static void publish_authority(noah_profile_split_reconciler_t *reconciler) {
    (void)noah_profile_split_authority_publish(&reconciler->authority, reconciler->local_descriptor, reconciler->peer_descriptor, transfer_pending(reconciler));
}

static noah_profile_split_v1_frame_t transfer_reply(noah_profile_split_v1_kind_t kind, noah_profile_split_v1_status_t status, uint32_t generation, uint32_t digest, uint16_t offset, uint16_t length) {
    return (noah_profile_split_v1_frame_t){
        .kind           = kind,
        .status         = status,
        .generation     = generation,
        .payload_digest = digest,
        .offset         = offset,
        .payload_length = length,
    };
}

static void frame_correlation(const noah_profile_split_v1_frame_t *frame, uint32_t *generation, uint32_t *digest, uint16_t *offset, uint16_t *length) {
    if (frame->kind == NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN || frame->kind == NOAH_PROFILE_SPLIT_V1_PREPARE_DURABLE || frame->kind == NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT || frame->kind == NOAH_PROFILE_SPLIT_V1_ABORT) {
        *generation = frame->descriptor.generation;
        *digest     = frame->descriptor.payload_digest;
        *offset     = 0u;
        *length     = frame->descriptor.payload_length;
    } else {
        *generation = frame->generation;
        *digest     = frame->payload_digest;
        *offset     = frame->offset;
        *length     = frame->payload_length;
    }
}

static void process_logical_bind(noah_profile_split_reconciler_t *reconciler, const uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], const noah_profile_split_v1_frame_t *request) {
    noah_profile_split_v1_frame_t response;

    reconciler->incoming_logical_binding = true;
    reconciler->incoming_profile_generation = request->generation;
    reconciler->incoming_profile_digest     = request->payload_digest;
    reconciler->incoming_via_generation  = request->via_generation;
    reconciler->incoming_via_digest      = request->via_digest;
    response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_OK, request->generation, request->payload_digest, 0u, 0u);
    cache_response(reconciler, request_wire, &response);
}

static void process_logical_bind_request(noah_profile_split_reconciler_t *reconciler, const uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], const noah_profile_split_v1_frame_t *request) {
    noah_profile_split_v1_frame_t response;
    uint32_t via_generation = 0u;
    uint32_t via_digest = 0u;

    refresh_local(reconciler);
    refresh_metadata_response(reconciler);
    if (!reconciler->local_descriptor.logical || reconciler->local_descriptor.generation != request->generation || reconciler->local_descriptor.payload_digest != request->payload_digest || !reconciler->config.local_binding || !reconciler->config.local_binding(reconciler->config.local_context, &reconciler->local_descriptor, &via_generation, &via_digest)) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ERROR, NOAH_PROFILE_SPLIT_V1_STATUS_STALE, request->generation, request->payload_digest, 0u, 0u);
    } else {
        response = (noah_profile_split_v1_frame_t){
            .kind                 = NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND,
            .status               = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
            .generation           = request->generation,
            .payload_digest       = request->payload_digest,
            .store_format_version = NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL,
            .via_generation       = via_generation,
            .via_digest           = via_digest,
        };
    }
    cache_response(reconciler, request_wire, &response);
    publish_authority(reconciler);
}

static bool encode_invalid_response(uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    noah_profile_split_v1_frame_t error = transfer_reply(NOAH_PROFILE_SPLIT_V1_ERROR, NOAH_PROFILE_SPLIT_V1_STATUS_INVALID_FRAME, 0u, 0u, 0u, 0u);
    return noah_profile_split_v1_frame_encode(&error, response);
}

static bool encode_busy_response(const noah_profile_split_v1_frame_t *request, uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    uint32_t generation;
    uint32_t digest;
    uint16_t offset;
    uint16_t length;

    frame_correlation(request, &generation, &digest, &offset, &length);
    return noah_profile_split_v1_frame_encode(&(noah_profile_split_v1_frame_t){
        .kind           = NOAH_PROFILE_SPLIT_V1_ACK,
        .status         = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY,
        .generation     = generation,
        .payload_digest = digest,
        .offset         = offset,
        .payload_length = length,
    }, response);
}

static void cache_response(noah_profile_split_reconciler_t *reconciler, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], const noah_profile_split_v1_frame_t *response) {
    uint8_t encoded[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];

    if (!noah_profile_split_v1_frame_encode(response, encoded)) {
        (void)encode_invalid_response(encoded);
    }
    noah_runtime_publication_begin(&reconciler->response_sequence);
    reconciler->cached_response_valid = false;
    memcpy(reconciler->cached_request_wire, request, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    memcpy(reconciler->cached_response_wire, encoded, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
    reconciler->cached_response_valid = true;
    noah_runtime_publication_end(&reconciler->response_sequence);
}

static void invalidate_cached_response(noah_profile_split_reconciler_t *reconciler) {
    noah_runtime_publication_begin(&reconciler->response_sequence);
    reconciler->cached_response_valid = false;
    noah_runtime_publication_end(&reconciler->response_sequence);
}

static bool read_cached_response(const noah_profile_split_reconciler_t *reconciler, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    for (uint8_t attempt = 0u; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t observed = noah_runtime_publication_observe(&reconciler->response_sequence);
        bool    matches;

        if (noah_runtime_publication_in_flight(observed)) {
            continue;
        }
        matches = reconciler->cached_response_valid && memcmp(reconciler->cached_request_wire, request, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE) == 0;
        if (matches) {
            memcpy(response, reconciler->cached_response_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE);
        }
        if (noah_runtime_publication_settled(&reconciler->response_sequence, observed)) {
            return matches;
        }
    }
    return false;
}

static noah_profile_split_v1_status_t map_peer_error(noah_profile_peer_store_result_t result) {
    switch (result) {
        case NOAH_PROFILE_PEER_STORE_STALE:
            return NOAH_PROFILE_SPLIT_V1_STATUS_STALE;
        case NOAH_PROFILE_PEER_STORE_CONFLICT:
            return NOAH_PROFILE_SPLIT_V1_STATUS_CONFLICT;
        case NOAH_PROFILE_PEER_STORE_CORRUPT:
        case NOAH_PROFILE_PEER_STORE_CHUNK_CONFLICT:
            return NOAH_PROFILE_SPLIT_V1_STATUS_CORRUPT;
        case NOAH_PROFILE_PEER_STORE_INCOMPATIBLE:
            return NOAH_PROFILE_SPLIT_V1_STATUS_INCOMPATIBLE;
        case NOAH_PROFILE_PEER_STORE_INVALID_METADATA:
            return NOAH_PROFILE_SPLIT_V1_STATUS_INVALID_FRAME;
        case NOAH_PROFILE_PEER_STORE_RANGE_ERROR:
            return NOAH_PROFILE_SPLIT_V1_STATUS_RANGE_ERROR;
        case NOAH_PROFILE_PEER_STORE_VALIDATION_ERROR:
            return NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR;
        case NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN:
        case NOAH_PROFILE_PEER_STORE_STORAGE_ERROR:
        default:
            return NOAH_PROFILE_SPLIT_V1_STATUS_STORAGE_ERROR;
    }
}

static noah_profile_split_v1_frame_t peer_error_reply(noah_profile_peer_store_result_t result, uint32_t generation, uint32_t digest, uint16_t offset, uint16_t length) {
    return transfer_reply(NOAH_PROFILE_SPLIT_V1_ERROR, map_peer_error(result), generation, digest, offset, length);
}

static bool response_busy(const noah_profile_split_v1_frame_t *response) {
    return response && response->kind == NOAH_PROFILE_SPLIT_V1_ACK && response->status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
}

static bool response_ack_matches(const noah_profile_split_v1_frame_t *response, const noah_profile_split_descriptor_t *descriptor) {
    return response && descriptor && response->kind == NOAH_PROFILE_SPLIT_V1_ACK && response->status == NOAH_PROFILE_SPLIT_V1_STATUS_OK && response->generation == descriptor->generation && response->payload_digest == descriptor->payload_digest && response->payload_length == descriptor->payload_length;
}

bool noah_profile_split_reconciler_receive(noah_profile_split_reconciler_t *reconciler, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    noah_profile_split_v1_frame_t decoded;

    if (!(reconciler && reconciler->initialized && request && response && request_length == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE && response_length == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE)) {
        return false;
    }
    if (!noah_profile_split_v1_frame_decode(request, request_length, &decoded)) {
        return encode_invalid_response(response);
    }
    reconciler->peer_activity_sequence++;
    NOAH_RUNTIME_PUBLICATION_BARRIER();
    if (decoded.kind == NOAH_PROFILE_SPLIT_V1_METADATA) {
        (void)publish_mailbox(reconciler, request);
        if (read_metadata_response(reconciler, response)) {
            return true;
        }
        return encode_busy_response(&decoded, response);
    }
    if (read_cached_response(reconciler, request, response)) {
        return true;
    }
    (void)publish_mailbox(reconciler, request);
    return encode_busy_response(&decoded, response);
}

static void process_metadata(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_v1_frame_t *request) {
    reconciler->peer_descriptor = request->descriptor;
    clear_provisional_peer(reconciler, &request->descriptor);
    publish_authority(reconciler);
}

static void process_prepare_begin(noah_profile_split_reconciler_t *reconciler, const uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], const noah_profile_split_v1_frame_t *request) {
    noah_profile_peer_store_result_t result;
    noah_profile_split_v1_frame_t    response;
    noah_profile_peer_store_state_t  store_state;

    observe_provisional_peer(reconciler, &request->descriptor);
    if (reconciler->transfer_owner == NOAH_PROFILE_SPLIT_TRANSFER_LOCAL_PULL) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, request->descriptor.generation, request->descriptor.payload_digest, 0u, request->descriptor.payload_length);
        cache_response(reconciler, request_wire, &response);
        publish_authority(reconciler);
        return;
    }
    if (reconciler->incoming_logical_binding && reconciler->incoming_profile_generation == request->descriptor.generation && reconciler->incoming_profile_digest == request->descriptor.payload_digest) {
        result = noah_profile_peer_store_backend_begin_logical(reconciler->config.peer_store, &request->descriptor, reconciler->incoming_via_generation, reconciler->incoming_via_digest);
    } else {
        result = noah_profile_peer_store_backend_begin(reconciler->config.peer_store, &request->descriptor);
    }
    reconciler->incoming_logical_binding = false;
    reconciler->incoming_profile_generation = 0u;
    reconciler->incoming_profile_digest     = 0u;
    reconciler->incoming_via_generation  = 0u;
    reconciler->incoming_via_digest      = 0u;
    store_state = noah_profile_peer_store_backend_state(reconciler->config.peer_store);
    if (result == NOAH_PROFILE_PEER_STORE_OK || result == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED || (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS && store_state == NOAH_PROFILE_PEER_STORE_RECEIVING)) {
        uint16_t next_offset = noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store);
        response             = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_OK, request->descriptor.generation, request->descriptor.payload_digest, next_offset, request->descriptor.payload_length);
        reconciler->transfer_owner = store_state == NOAH_PROFILE_PEER_STORE_RECEIVING ? NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH : NOAH_PROFILE_SPLIT_TRANSFER_NONE;
    } else if (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS || result == NOAH_PROFILE_PEER_STORE_BUSY) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, request->descriptor.generation, request->descriptor.payload_digest, noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store), request->descriptor.payload_length);
    } else {
        response                = peer_error_reply(result, request->descriptor.generation, request->descriptor.payload_digest, 0u, request->descriptor.payload_length);
        reconciler->last_status = response.status;
    }
    cache_response(reconciler, request_wire, &response);
    publish_authority(reconciler);
}

static void process_prepare_durable(noah_profile_split_reconciler_t *reconciler, const uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], const noah_profile_split_v1_frame_t *request) {
    noah_profile_peer_store_result_t result;
    noah_profile_split_v1_frame_t response;

    result = noah_profile_peer_store_backend_prepare_durable_begin(reconciler->config.peer_store, &request->descriptor);
    if (result == NOAH_PROFILE_PEER_STORE_OK || result == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_OK, request->descriptor.generation, request->descriptor.payload_digest, request->descriptor.payload_length, request->descriptor.payload_length);
    } else if (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS || result == NOAH_PROFILE_PEER_STORE_BUSY) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, request->descriptor.generation, request->descriptor.payload_digest, noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store), request->descriptor.payload_length);
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH;
    } else {
        response = peer_error_reply(result, request->descriptor.generation, request->descriptor.payload_digest, 0u, request->descriptor.payload_length);
        reconciler->last_status = response.status;
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
    }
    cache_response(reconciler, request_wire, &response);
    publish_authority(reconciler);
}

static void process_payload_chunk(noah_profile_split_reconciler_t *reconciler, const uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], const noah_profile_split_v1_frame_t *request) {
    noah_profile_peer_store_result_t result;
    noah_profile_split_v1_frame_t    response;

    if (reconciler->transfer_owner != NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH || !descriptor_correlation_matches(&reconciler->config.peer_store->descriptor, request->generation, request->payload_digest, request->payload_length)) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, request->generation, request->payload_digest, noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store), request->payload_length);
        cache_response(reconciler, request_wire, &response);
        publish_authority(reconciler);
        return;
    }
    result = noah_profile_peer_store_backend_write(reconciler->config.peer_store, request->generation, request->payload_digest, request->offset, request->chunk, request->chunk_length);
    if (result == NOAH_PROFILE_PEER_STORE_OK) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_OK, request->generation, request->payload_digest, noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store), request->payload_length);
    } else if (result == NOAH_PROFILE_PEER_STORE_BUSY || result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, request->generation, request->payload_digest, noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store), request->payload_length);
    } else {
        response                = peer_error_reply(result, request->generation, request->payload_digest, request->offset, request->payload_length);
        reconciler->last_status = response.status;
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
    }
    cache_response(reconciler, request_wire, &response);
    publish_authority(reconciler);
}

static void process_prepare_commit(noah_profile_split_reconciler_t *reconciler, const uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], const noah_profile_split_v1_frame_t *request) {
    noah_profile_peer_store_result_t result;
    noah_profile_split_v1_frame_t    response;

    if (reconciler->transfer_owner != NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH && noah_profile_peer_store_backend_state(reconciler->config.peer_store) != NOAH_PROFILE_PEER_STORE_PREPARED && noah_profile_peer_store_backend_state(reconciler->config.peer_store) != NOAH_PROFILE_PEER_STORE_COMMITTED) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, request->descriptor.generation, request->descriptor.payload_digest, noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store), request->descriptor.payload_length);
        cache_response(reconciler, request_wire, &response);
        publish_authority(reconciler);
        return;
    }
    result = noah_profile_peer_store_backend_state(reconciler->config.peer_store) == NOAH_PROFILE_PEER_STORE_PREPARED ? noah_profile_peer_store_backend_prepared_commit_begin(reconciler->config.peer_store, &request->descriptor) : noah_profile_peer_store_backend_commit_begin(reconciler->config.peer_store, &request->descriptor);
    if (result == NOAH_PROFILE_PEER_STORE_OK || result == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_OK, request->descriptor.generation, request->descriptor.payload_digest, request->descriptor.payload_length, request->descriptor.payload_length);
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
        clear_provisional_peer(reconciler, &request->descriptor);
    } else if (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS || result == NOAH_PROFILE_PEER_STORE_BUSY) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, request->descriptor.generation, request->descriptor.payload_digest, noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store), request->descriptor.payload_length);
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH;
    } else {
        response                = peer_error_reply(result, request->descriptor.generation, request->descriptor.payload_digest, 0u, request->descriptor.payload_length);
        reconciler->last_status = response.status;
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
    }
    cache_response(reconciler, request_wire, &response);
    refresh_local(reconciler);
    refresh_metadata_response(reconciler);
    publish_authority(reconciler);
}

static void process_abort(noah_profile_split_reconciler_t *reconciler, const uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], const noah_profile_split_v1_frame_t *request) {
    noah_profile_peer_store_result_t result;
    noah_profile_split_v1_frame_t    response;
    noah_profile_peer_store_state_t  state = noah_profile_peer_store_backend_state(reconciler->config.peer_store);
    bool                             matches_live = descriptor_equal(&reconciler->config.peer_store->descriptor, &request->descriptor);

    clear_provisional_peer(reconciler, &request->descriptor);
    if ((state == NOAH_PROFILE_PEER_STORE_RECEIVING || state == NOAH_PROFILE_PEER_STORE_VALIDATING || state == NOAH_PROFILE_PEER_STORE_PREPARED) && matches_live) {
        result = noah_profile_peer_store_backend_abort(reconciler->config.peer_store, &request->descriptor);
    } else if (state == NOAH_PROFILE_PEER_STORE_PREPARING || state == NOAH_PROFILE_PEER_STORE_COMMITTING || state == NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED || ((state == NOAH_PROFILE_PEER_STORE_RECEIVING || state == NOAH_PROFILE_PEER_STORE_VALIDATING || state == NOAH_PROFILE_PEER_STORE_PREPARED) && !matches_live)) {
        result = NOAH_PROFILE_PEER_STORE_BUSY;
    } else if (state == NOAH_PROFILE_PEER_STORE_COMMITTED && matches_live) {
        // A no-op abort is successful only when the descriptor was never
        // admitted provisionally. Never report success for matching durable
        // state because the sender cannot safely infer that no marker landed.
        result = NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED;
    } else {
        // IDLE/REJECTED or an unrelated durable descriptor proves that this
        // exact provisional lease is absent. ACK makes cancellation
        // idempotent after rejection or a lost request-before-delivery.
        result = NOAH_PROFILE_PEER_STORE_OK;
    }
    if (result == NOAH_PROFILE_PEER_STORE_OK) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_OK, request->descriptor.generation, request->descriptor.payload_digest, 0u, request->descriptor.payload_length);
        if (matches_live) {
            reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
        }
    } else if (result == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED) {
        response                = transfer_reply(NOAH_PROFILE_SPLIT_V1_ERROR, NOAH_PROFILE_SPLIT_V1_STATUS_CONFLICT, request->descriptor.generation, request->descriptor.payload_digest, 0u, request->descriptor.payload_length);
        reconciler->last_status = response.status;
    } else if (result == NOAH_PROFILE_PEER_STORE_BUSY || result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_BUSY, request->descriptor.generation, request->descriptor.payload_digest, noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store), request->descriptor.payload_length);
    } else {
        response = peer_error_reply(result, request->descriptor.generation, request->descriptor.payload_digest, 0u, request->descriptor.payload_length);
    }
    cache_response(reconciler, request_wire, &response);
    publish_authority(reconciler);
}

static void process_payload_request(noah_profile_split_reconciler_t *reconciler, const uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], const noah_profile_split_v1_frame_t *request) {
    noah_profile_split_v1_frame_t response;
    uint16_t                      remaining;

    refresh_local(reconciler);
    refresh_metadata_response(reconciler);
    if (!descriptor_correlation_matches(&reconciler->local_descriptor, request->generation, request->payload_digest, request->payload_length)) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ERROR, NOAH_PROFILE_SPLIT_V1_STATUS_STALE, request->generation, request->payload_digest, request->offset, request->payload_length);
    } else {
        remaining = (uint16_t)(request->payload_length - request->offset);
        response  = (noah_profile_split_v1_frame_t){
            .kind           = NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK,
            .status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
            .generation     = request->generation,
            .payload_digest = request->payload_digest,
            .offset         = request->offset,
            .payload_length = request->payload_length,
            .chunk_length   = remaining < NOAH_PROFILE_SPLIT_V1_CHUNK_MAX ? (uint8_t)remaining : NOAH_PROFILE_SPLIT_V1_CHUNK_MAX,
        };
        if (!reconciler->config.local_read || !reconciler->config.local_read(reconciler->config.local_context, &reconciler->local_descriptor, response.offset, response.chunk, response.chunk_length)) {
            response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ERROR, NOAH_PROFILE_SPLIT_V1_STATUS_STORAGE_ERROR, request->generation, request->payload_digest, request->offset, request->payload_length);
        }
    }
    cache_response(reconciler, request_wire, &response);
    publish_authority(reconciler);
}

static void process_mailbox(noah_profile_split_reconciler_t *reconciler, noah_profile_split_reconcile_mode_t mode) {
    uint8_t                       request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    noah_profile_split_v1_frame_t request;

    if (!consume_mailbox(reconciler, request_wire)) {
        return;
    }
    if (!noah_profile_split_v1_frame_decode(request_wire, sizeof(request_wire), &request)) {
        return;
    }
    if (request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN) {
        observe_provisional_peer(reconciler, &request.descriptor);
    } else if (request.kind == NOAH_PROFILE_SPLIT_V1_ABORT) {
        clear_provisional_peer(reconciler, &request.descriptor);
    }
    if (mode == NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY && request.kind != NOAH_PROFILE_SPLIT_V1_METADATA && request.kind != NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST && request.kind != NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND_REQUEST && request.kind != NOAH_PROFILE_SPLIT_V1_ABORT) {
        cache_response(reconciler, request_wire, &(noah_profile_split_v1_frame_t){
            .kind           = NOAH_PROFILE_SPLIT_V1_ACK,
            .status         = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY,
            .generation     = request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN || request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_DURABLE || request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT || request.kind == NOAH_PROFILE_SPLIT_V1_ABORT ? request.descriptor.generation : request.generation,
            .payload_digest = request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN || request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_DURABLE || request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT || request.kind == NOAH_PROFILE_SPLIT_V1_ABORT ? request.descriptor.payload_digest : request.payload_digest,
            .offset         = request.offset,
            .payload_length = request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN || request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_DURABLE || request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT || request.kind == NOAH_PROFILE_SPLIT_V1_ABORT ? request.descriptor.payload_length : request.payload_length,
        });
        publish_authority(reconciler);
        return;
    }
    switch (request.kind) {
        case NOAH_PROFILE_SPLIT_V1_METADATA:
            process_metadata(reconciler, &request);
            break;
        case NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN:
            process_prepare_begin(reconciler, request_wire, &request);
            break;
        case NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND:
            process_logical_bind(reconciler, request_wire, &request);
            break;
        case NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND_REQUEST:
            process_logical_bind_request(reconciler, request_wire, &request);
            break;
        case NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK:
            process_payload_chunk(reconciler, request_wire, &request);
            break;
        case NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT:
            process_prepare_commit(reconciler, request_wire, &request);
            break;
        case NOAH_PROFILE_SPLIT_V1_PREPARE_DURABLE:
            process_prepare_durable(reconciler, request_wire, &request);
            break;
        case NOAH_PROFILE_SPLIT_V1_ABORT:
            process_abort(reconciler, request_wire, &request);
            break;
        case NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST:
            process_payload_request(reconciler, request_wire, &request);
            break;
        case NOAH_PROFILE_SPLIT_V1_ACK:
        case NOAH_PROFILE_SPLIT_V1_ERROR:
        default:
            cache_response(reconciler, request_wire, &(noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_ERROR, .status = NOAH_PROFILE_SPLIT_V1_STATUS_INVALID_FRAME});
            break;
    }
}

static void update_remote_commit_response(noah_profile_split_reconciler_t *reconciler, noah_profile_peer_store_result_t result) {
    noah_profile_split_v1_frame_t request;
    noah_profile_split_v1_frame_t response;
    uint8_t                       request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];

    if (!reconciler->cached_response_valid || !noah_profile_split_v1_frame_decode(reconciler->cached_request_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, &request) || (request.kind != NOAH_PROFILE_SPLIT_V1_PREPARE_DURABLE && request.kind != NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT)) {
        return;
    }
    memcpy(request_wire, reconciler->cached_request_wire, sizeof(request_wire));
    if (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS) {
        return;
    }
    if (result == NOAH_PROFILE_PEER_STORE_OK || result == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED) {
        response = transfer_reply(NOAH_PROFILE_SPLIT_V1_ACK, NOAH_PROFILE_SPLIT_V1_STATUS_OK, request.descriptor.generation, request.descriptor.payload_digest, request.descriptor.payload_length, request.descriptor.payload_length);
        if (request.kind == NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT) {
            clear_provisional_peer(reconciler, &request.descriptor);
        }
    } else {
        response                = peer_error_reply(result, request.descriptor.generation, request.descriptor.payload_digest, 0u, request.descriptor.payload_length);
        reconciler->last_status = response.status;
    }
    cache_response(reconciler, request_wire, &response);
}

static void release_transient_busy_response(noah_profile_split_reconciler_t *reconciler) {
    noah_profile_split_v1_frame_t response;
    noah_profile_peer_store_state_t state;

    if (!reconciler->cached_response_valid || !noah_profile_split_v1_frame_decode(reconciler->cached_response_wire, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, &response) || !response_busy(&response)) {
        return;
    }
    state = noah_profile_peer_store_backend_state(reconciler->config.peer_store);
    if (state == NOAH_PROFILE_PEER_STORE_VALIDATING || state == NOAH_PROFILE_PEER_STORE_PREPARING || state == NOAH_PROFILE_PEER_STORE_COMMITTING) {
        return;
    }
    // BUSY is cached long enough for one byte-identical retry to observe it.
    // When no asynchronous validator/commit step can replace that response,
    // release it at the next scan so a later retry can be re-admitted.
    noah_runtime_publication_begin(&reconciler->response_sequence);
    reconciler->cached_response_valid = false;
    noah_runtime_publication_end(&reconciler->response_sequence);
}

static bool advance_store_transfer(noah_profile_split_reconciler_t *reconciler) {
    noah_profile_peer_store_state_t  state  = noah_profile_peer_store_backend_state(reconciler->config.peer_store);
    noah_profile_peer_store_result_t result;

    if (state != NOAH_PROFILE_PEER_STORE_VALIDATING && state != NOAH_PROFILE_PEER_STORE_PREPARING && state != NOAH_PROFILE_PEER_STORE_COMMITTING) {
        return false;
    }
    result = noah_profile_peer_store_backend_step(reconciler->config.peer_store, NOAH_PROFILE_SPLIT_V1_CHUNK_MAX);
    if (reconciler->transfer_owner == NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH) {
        update_remote_commit_response(reconciler, result);
    }
    if (result != NOAH_PROFILE_PEER_STORE_IN_PROGRESS) {
        reconciler->transfer_owner = noah_profile_peer_store_backend_state(reconciler->config.peer_store) == NOAH_PROFILE_PEER_STORE_PREPARED ? NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH : NOAH_PROFILE_SPLIT_TRANSFER_NONE;
        refresh_local(reconciler);
        refresh_metadata_response(reconciler);
        publish_authority(reconciler);
    }
    return true;
}

static bool expire_remote_prepare(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    noah_profile_peer_store_result_t result;

    if (!(reconciler && reconciler->transfer_owner == NOAH_PROFILE_SPLIT_TRANSFER_REMOTE_PUSH && reconciler->peer_activity_known && noah_profile_peer_store_backend_state(reconciler->config.peer_store) == NOAH_PROFILE_PEER_STORE_RECEIVING && deadline_reached(now, reconciler->last_peer_activity_at + NOAH_PROFILE_SPLIT_PREPARE_LEASE_MS))) {
        return false;
    }
    result = noah_profile_peer_store_backend_abort(reconciler->config.peer_store, &reconciler->config.peer_store->descriptor);
    if (result != NOAH_PROFILE_PEER_STORE_OK) {
        reconciler->last_status = map_peer_error(result);
        publish_authority(reconciler);
        return true;
    }
    reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
    invalidate_cached_response(reconciler);
    reconciler->peer_activity_known = false;
    invalidate_peer(reconciler);
    return true;
}

static void retry_later(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    reconciler->attempt_immediate = false;
    reconciler->next_attempt_at = now + reconciler->retry_ms;
    if (reconciler->retry_ms < NOAH_PROFILE_SPLIT_RETRY_MAX_MS) {
        uint32_t doubled = reconciler->retry_ms * 2u;
        reconciler->retry_ms = doubled < NOAH_PROFILE_SPLIT_RETRY_MAX_MS ? doubled : NOAH_PROFILE_SPLIT_RETRY_MAX_MS;
    }
    if (reconciler->retry_count != UINT32_MAX) {
        reconciler->retry_count++;
    }
}

static void retry_admitted_mailbox(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    if (reconciler->retry_ms != NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS) {
        retry_later(reconciler, now);
        return;
    }
    reconciler->attempt_immediate = false;
    reconciler->next_attempt_at   = now + NOAH_PROFILE_SPLIT_ADMISSION_RETRY_MS;
    reconciler->retry_ms          = NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS * 2u;
    if (reconciler->retry_count != UINT32_MAX) {
        reconciler->retry_count++;
    }
}

static void note_progress(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    reconciler->attempt_immediate = false;
    reconciler->retry_ms        = NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
    reconciler->next_attempt_at = now;
}

static void invalidate_peer(noah_profile_split_reconciler_t *reconciler) {
    reconciler->peer_descriptor = unreadable_descriptor();
    clear_provisional_peer(reconciler, NULL);
    publish_authority(reconciler);
}

static void transport_lost(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    if (reconciler->transport_failure_count != UINT32_MAX) {
        reconciler->transport_failure_count++;
    }
    reconciler->last_status = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
    reconciler->state       = reconciler->master ? NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER : NOAH_PROFILE_SPLIT_RECONCILER_PASSIVE;
    invalidate_peer(reconciler);
    retry_later(reconciler, now);
}

static bool rpc_exchange(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_v1_frame_t *request, noah_profile_split_v1_frame_t *response, uint32_t now) {
    uint8_t request_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];
    uint8_t response_wire[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];

    if (!reconciler->config.exchange || !noah_profile_split_v1_frame_encode(request, request_wire) || !reconciler->config.exchange(reconciler->config.transport_context, request_wire, response_wire) || !noah_profile_split_v1_frame_decode(response_wire, sizeof(response_wire), response)) {
        transport_lost(reconciler, now);
        return false;
    }
    return true;
}

static void stop_with_status(noah_profile_split_reconciler_t *reconciler, noah_profile_split_v1_status_t status) {
    reconciler->last_status              = status;
    reconciler->stopped_local_descriptor = reconciler->local_descriptor;
    reconciler->state                    = NOAH_PROFILE_SPLIT_RECONCILER_STOPPED;
    publish_authority(reconciler);
}

static void handle_protocol_error(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_v1_frame_t *response, uint32_t now) {
    if (!response || response->kind != NOAH_PROFILE_SPLIT_V1_ERROR) {
        transport_lost(reconciler, now);
        return;
    }
    if (response->status == NOAH_PROFILE_SPLIT_V1_STATUS_STALE || response->status == NOAH_PROFILE_SPLIT_V1_STATUS_BUSY) {
        reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER;
        retry_later(reconciler, now);
        return;
    }
    stop_with_status(reconciler, response->status);
}

static void begin_authority_action(noah_profile_split_reconciler_t *reconciler, noah_profile_split_authority_state_t authority_state, uint32_t now, noah_profile_split_reconcile_mode_t mode) {
    reconciler->last_status = NOAH_PROFILE_SPLIT_V1_STATUS_OK;
    switch (authority_state) {
        case NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED:
        case NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED:
            if (reconciler->prepared_push_active && descriptor_equal(&reconciler->local_descriptor, &reconciler->transfer_descriptor) && descriptor_equal(&reconciler->peer_descriptor, &reconciler->transfer_descriptor)) {
                clear_prepared_push(reconciler);
            }
            reconciler->state           = NOAH_PROFILE_SPLIT_RECONCILER_CONVERGED;
            reconciler->next_attempt_at = now + NOAH_PROFILE_SPLIT_POLL_MS;
            publish_authority(reconciler);
            break;
        case NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_NEWER:
            reconciler->transfer_descriptor = reconciler->local_descriptor;
            reconciler->transfer_offset     = 0u;
            reconciler->prepared_logical    = reconciler->local_descriptor.logical && reconciler->config.local_binding && reconciler->config.local_binding(reconciler->config.local_context, &reconciler->local_descriptor, &reconciler->prepared_via_generation, &reconciler->prepared_via_digest);
            if (reconciler->local_descriptor.logical && !reconciler->prepared_logical) {
                stop_with_status(reconciler, NOAH_PROFILE_SPLIT_V1_STATUS_STORAGE_ERROR);
                break;
            }
            reconciler->state               = reconciler->prepared_logical ? NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BIND : NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN;
            publish_authority(reconciler);
            break;
        case NOAH_PROFILE_SPLIT_AUTHORITY_PEER_NEWER:
            if (mode == NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY) {
                reconciler->last_status = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
                reconciler->state       = NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER;
                retry_later(reconciler, now);
                publish_authority(reconciler);
                break;
            }
            reconciler->transfer_descriptor = reconciler->peer_descriptor;
            reconciler->transfer_offset     = 0u;
            reconciler->prepared_logical    = reconciler->peer_descriptor.logical;
            reconciler->prepared_via_generation = 0u;
            reconciler->prepared_via_digest = 0u;
            reconciler->state               = reconciler->prepared_logical ? NOAH_PROFILE_SPLIT_RECONCILER_PULL_BIND : NOAH_PROFILE_SPLIT_RECONCILER_PULL_BEGIN;
            publish_authority(reconciler);
            break;
        case NOAH_PROFILE_SPLIT_AUTHORITY_INCOMPATIBLE:
            stop_with_status(reconciler, NOAH_PROFILE_SPLIT_V1_STATUS_INCOMPATIBLE);
            break;
        case NOAH_PROFILE_SPLIT_AUTHORITY_CONCURRENT_COMMIT:
            stop_with_status(reconciler, NOAH_PROFILE_SPLIT_V1_STATUS_CONFLICT);
            break;
        case NOAH_PROFILE_SPLIT_AUTHORITY_CORRUPT_SAME_TUPLE:
            stop_with_status(reconciler, NOAH_PROFILE_SPLIT_V1_STATUS_CORRUPT);
            break;
        case NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_UNREADABLE:
        case NOAH_PROFILE_SPLIT_AUTHORITY_PEER_UNREADABLE:
        case NOAH_PROFILE_SPLIT_AUTHORITY_INVALID_METADATA:
        case NOAH_PROFILE_SPLIT_AUTHORITY_UNINITIALIZED:
        default:
            reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER;
            retry_later(reconciler, now);
            publish_authority(reconciler);
            break;
    }
}

static void metadata_exchange(noah_profile_split_reconciler_t *reconciler, uint32_t now, noah_profile_split_reconcile_mode_t mode) {
    noah_profile_split_v1_frame_t request;
    noah_profile_split_v1_frame_t response;

    refresh_local(reconciler);
    refresh_metadata_response(reconciler);
    request = (noah_profile_split_v1_frame_t){.kind = NOAH_PROFILE_SPLIT_V1_METADATA, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .descriptor = reconciler->local_descriptor};
    if (!rpc_exchange(reconciler, &request, &response, now)) {
        return;
    }
    if (response_busy(&response)) {
        retry_later(reconciler, now);
        return;
    }
    if (response.kind != NOAH_PROFILE_SPLIT_V1_METADATA || response.status != NOAH_PROFILE_SPLIT_V1_STATUS_OK) {
        handle_protocol_error(reconciler, &response, now);
        return;
    }
    reconciler->peer_descriptor = response.descriptor;
    note_progress(reconciler, now);
    begin_authority_action(reconciler, noah_profile_split_authority_compare(&reconciler->local_descriptor, &reconciler->peer_descriptor), now, mode);
}

static void push_bind(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    noah_profile_split_v1_frame_t response;
    noah_profile_split_v1_frame_t request = {
        .kind                 = NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND,
        .status               = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation           = reconciler->transfer_descriptor.generation,
        .payload_digest       = reconciler->transfer_descriptor.payload_digest,
        .store_format_version = NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL,
        .via_generation       = reconciler->prepared_via_generation,
        .via_digest           = reconciler->prepared_via_digest,
    };

    if (!rpc_exchange(reconciler, &request, &response, now)) {
        reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BIND;
        return;
    }
    if (response_busy(&response)) {
        retry_admitted_mailbox(reconciler, now);
        return;
    }
    if (response.kind != NOAH_PROFILE_SPLIT_V1_ACK || response.status != NOAH_PROFILE_SPLIT_V1_STATUS_OK || response.generation != request.generation || response.payload_digest != request.payload_digest || response.payload_length != 0u) {
        handle_protocol_error(reconciler, &response, now);
        return;
    }
    reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN;
    note_progress(reconciler, now);
}

static void push_begin(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    noah_profile_split_v1_frame_t response;
    noah_profile_split_v1_frame_t request = {.kind = NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .descriptor = reconciler->transfer_descriptor};

    if (reconciler->prepared_push_active) {
        // Once PREPARE_BEGIN is handed to the transport it may have reached the
        // peer even when its reply is lost, so cancellation must send ABORT.
        reconciler->prepared_remote_started = true;
    }
    if (!rpc_exchange(reconciler, &request, &response, now)) {
        if (reconciler->prepared_push_active) {
            reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN;
        }
        return;
    }
    if (response_busy(&response)) {
        retry_admitted_mailbox(reconciler, now);
        return;
    }
    if (!response_ack_matches(&response, &reconciler->transfer_descriptor) || response.offset > reconciler->transfer_descriptor.payload_length) {
        handle_protocol_error(reconciler, &response, now);
        return;
    }
    reconciler->transfer_offset = response.offset;
    reconciler->state           = response.offset == reconciler->transfer_descriptor.payload_length ? (reconciler->prepared_push_active ? prepared_payload_complete_state(reconciler) : NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT) : NOAH_PROFILE_SPLIT_RECONCILER_PUSH_READ;
    note_progress(reconciler, now);
}

static void push_read(noah_profile_split_reconciler_t *reconciler) {
    uint16_t remaining;
    void    *source_context;
    noah_profile_split_local_read_fn source_read;

    if (reconciler->prepared_push_active) {
        source_context = reconciler->prepared_source_context;
        source_read    = reconciler->prepared_source_read;
    } else {
        refresh_local(reconciler);
        refresh_metadata_response(reconciler);
        if (!descriptor_equal(&reconciler->local_descriptor, &reconciler->transfer_descriptor)) {
            reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER;
            publish_authority(reconciler);
            return;
        }
        source_context = reconciler->config.local_context;
        source_read    = reconciler->config.local_read;
    }
    remaining = (uint16_t)(reconciler->transfer_descriptor.payload_length - reconciler->transfer_offset);
    reconciler->outbound_chunk_length = remaining < NOAH_PROFILE_SPLIT_V1_CHUNK_MAX ? (uint8_t)remaining : NOAH_PROFILE_SPLIT_V1_CHUNK_MAX;
    if (!source_read || !source_read(source_context, &reconciler->transfer_descriptor, reconciler->transfer_offset, reconciler->outbound_chunk, reconciler->outbound_chunk_length)) {
        stop_with_status(reconciler, NOAH_PROFILE_SPLIT_V1_STATUS_STORAGE_ERROR);
        return;
    }
    reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_SEND;
}

static void push_send(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    noah_profile_split_v1_frame_t response;
    noah_profile_split_v1_frame_t request = {
        .kind           = NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK,
        .status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation     = reconciler->transfer_descriptor.generation,
        .payload_digest = reconciler->transfer_descriptor.payload_digest,
        .offset         = reconciler->transfer_offset,
        .payload_length = reconciler->transfer_descriptor.payload_length,
        .chunk_length   = reconciler->outbound_chunk_length,
    };
    uint16_t expected_offset = (uint16_t)(reconciler->transfer_offset + reconciler->outbound_chunk_length);

    memcpy(request.chunk, reconciler->outbound_chunk, reconciler->outbound_chunk_length);
    if (!rpc_exchange(reconciler, &request, &response, now)) {
        if (reconciler->prepared_push_active) {
            reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_SEND;
        }
        return;
    }
    if (response_busy(&response)) {
        retry_admitted_mailbox(reconciler, now);
        return;
    }
    if (!response_ack_matches(&response, &reconciler->transfer_descriptor) || response.offset != expected_offset) {
        handle_protocol_error(reconciler, &response, now);
        return;
    }
    reconciler->transfer_offset = response.offset;
    reconciler->state           = response.offset == reconciler->transfer_descriptor.payload_length ? (reconciler->prepared_push_active ? prepared_payload_complete_state(reconciler) : NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT) : NOAH_PROFILE_SPLIT_RECONCILER_PUSH_READ;
    note_progress(reconciler, now);
}

static void push_commit(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    noah_profile_split_v1_frame_t response;
    noah_profile_split_v1_frame_t request = {.kind = NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .descriptor = reconciler->transfer_descriptor};

    if (!rpc_exchange(reconciler, &request, &response, now)) {
        if (reconciler->prepared_push_active) {
            reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT;
        }
        return;
    }
    if (response_busy(&response)) {
        if (reconciler->prepared_push_active && reconciler->retry_ms > NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS && response.generation == reconciler->transfer_descriptor.generation && response.payload_digest == reconciler->transfer_descriptor.payload_digest && response.payload_length == reconciler->transfer_descriptor.payload_length && response.offset < reconciler->transfer_descriptor.payload_length) {
            // The callback's first BUSY is only mailbox admission. A repeated
            // correlated BUSY at a short offset proves the receiver no longer
            // holds the fully staged lease, so rebuild it before retrying the
            // already-authorized marker-last commit.
            reconciler->transfer_offset = 0u;
            reconciler->state           = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN;
            note_progress(reconciler, now);
            publish_authority(reconciler);
            return;
        }
        retry_admitted_mailbox(reconciler, now);
        return;
    }
    if (!response_ack_matches(&response, &reconciler->transfer_descriptor)) {
        handle_protocol_error(reconciler, &response, now);
        return;
    }
    reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_VERIFY;
    note_progress(reconciler, now);
    publish_authority(reconciler);
}

static void push_durable(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    noah_profile_split_v1_frame_t response;
    noah_profile_split_v1_frame_t request = {.kind = NOAH_PROFILE_SPLIT_V1_PREPARE_DURABLE, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .descriptor = reconciler->transfer_descriptor};

    if (!rpc_exchange(reconciler, &request, &response, now)) {
        reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_DURABLE;
        return;
    }
    if (response_busy(&response)) {
        retry_admitted_mailbox(reconciler, now);
        return;
    }
    if (!response_ack_matches(&response, &reconciler->transfer_descriptor) || response.offset != reconciler->transfer_descriptor.payload_length) {
        handle_protocol_error(reconciler, &response, now);
        return;
    }
    reconciler->state = reconciler->prepared_commit_authorized ? NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT : NOAH_PROFILE_SPLIT_RECONCILER_PUSH_PREPARED;
    note_progress(reconciler, now);
    publish_authority(reconciler);
}

static void push_abort(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    noah_profile_split_v1_frame_t response;
    noah_profile_split_v1_frame_t request = {.kind = NOAH_PROFILE_SPLIT_V1_ABORT, .status = NOAH_PROFILE_SPLIT_V1_STATUS_OK, .descriptor = reconciler->transfer_descriptor};

    if (!rpc_exchange(reconciler, &request, &response, now)) {
        reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_ABORT;
        return;
    }
    if (response_busy(&response)) {
        retry_admitted_mailbox(reconciler, now);
        return;
    }
    if (!response_ack_matches(&response, &reconciler->transfer_descriptor)) {
        reconciler->prepared_cancel_refused = true;
        handle_protocol_error(reconciler, &response, now);
        return;
    }
    clear_prepared_push(reconciler);
    reconciler->transfer_offset     = 0u;
    reconciler->state               = NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER;
    note_progress(reconciler, now);
    publish_authority(reconciler);
}

static void pull_bind(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    noah_profile_split_v1_frame_t response;
    noah_profile_split_v1_frame_t request = {
        .kind           = NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND_REQUEST,
        .status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation     = reconciler->transfer_descriptor.generation,
        .payload_digest = reconciler->transfer_descriptor.payload_digest,
    };

    if (!rpc_exchange(reconciler, &request, &response, now)) {
        reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PULL_BIND;
        return;
    }
    if (response_busy(&response)) {
        retry_admitted_mailbox(reconciler, now);
        return;
    }
    if (response.kind != NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND || response.status != NOAH_PROFILE_SPLIT_V1_STATUS_OK || response.generation != request.generation || response.payload_digest != request.payload_digest || response.store_format_version != NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL || response.via_generation == 0u || response.via_digest == 0u) {
        handle_protocol_error(reconciler, &response, now);
        return;
    }
    reconciler->prepared_via_generation = response.via_generation;
    reconciler->prepared_via_digest     = response.via_digest;
    reconciler->state                   = NOAH_PROFILE_SPLIT_RECONCILER_PULL_BEGIN;
    note_progress(reconciler, now);
}

static void pull_begin(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    noah_profile_peer_store_result_t result;
    noah_profile_peer_store_state_t  state = noah_profile_peer_store_backend_state(reconciler->config.peer_store);

    if (reconciler->transfer_owner == NOAH_PROFILE_SPLIT_TRANSFER_LOCAL_PULL && (state == NOAH_PROFILE_PEER_STORE_RECEIVING || state == NOAH_PROFILE_PEER_STORE_VALIDATING) && !descriptor_equal(&reconciler->config.peer_store->descriptor, &reconciler->transfer_descriptor)) {
        result = noah_profile_peer_store_backend_abort(reconciler->config.peer_store, &reconciler->config.peer_store->descriptor);
        if (result != NOAH_PROFILE_PEER_STORE_OK) {
            stop_with_status(reconciler, map_peer_error(result));
        } else {
            reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
        }
        publish_authority(reconciler);
        return;
    }

    result = reconciler->prepared_logical
                 ? noah_profile_peer_store_backend_begin_logical(reconciler->config.peer_store, &reconciler->transfer_descriptor, reconciler->prepared_via_generation, reconciler->prepared_via_digest)
                 : noah_profile_peer_store_backend_begin(reconciler->config.peer_store, &reconciler->transfer_descriptor);
    state  = noah_profile_peer_store_backend_state(reconciler->config.peer_store);

    if (result == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED) {
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
        reconciler->state          = NOAH_PROFILE_SPLIT_RECONCILER_VERIFY;
    } else if (result == NOAH_PROFILE_PEER_STORE_OK || (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS && state == NOAH_PROFILE_PEER_STORE_RECEIVING)) {
        reconciler->transfer_owner  = NOAH_PROFILE_SPLIT_TRANSFER_LOCAL_PULL;
        reconciler->transfer_offset = noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store);
        reconciler->state           = reconciler->transfer_offset == reconciler->transfer_descriptor.payload_length ? NOAH_PROFILE_SPLIT_RECONCILER_PULL_COMMIT_BEGIN : NOAH_PROFILE_SPLIT_RECONCILER_PULL_REQUEST;
    } else if (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS && (state == NOAH_PROFILE_PEER_STORE_VALIDATING || state == NOAH_PROFILE_PEER_STORE_COMMITTING)) {
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_LOCAL_PULL;
        reconciler->state          = NOAH_PROFILE_SPLIT_RECONCILER_PULL_COMMIT_STEP;
    } else if (result == NOAH_PROFILE_PEER_STORE_BUSY) {
        reconciler->last_status = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
        retry_later(reconciler, now);
    } else {
        stop_with_status(reconciler, map_peer_error(result));
    }
    publish_authority(reconciler);
}

static void pull_request(noah_profile_split_reconciler_t *reconciler, uint32_t now) {
    noah_profile_split_v1_frame_t response;
    noah_profile_split_v1_frame_t request = {
        .kind           = NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST,
        .status         = NOAH_PROFILE_SPLIT_V1_STATUS_OK,
        .generation     = reconciler->transfer_descriptor.generation,
        .payload_digest = reconciler->transfer_descriptor.payload_digest,
        .offset         = reconciler->transfer_offset,
        .payload_length = reconciler->transfer_descriptor.payload_length,
    };

    if (!rpc_exchange(reconciler, &request, &response, now)) {
        return;
    }
    if (response_busy(&response)) {
        retry_admitted_mailbox(reconciler, now);
        return;
    }
    if (response.kind != NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK || response.status != NOAH_PROFILE_SPLIT_V1_STATUS_OK || response.generation != request.generation || response.payload_digest != request.payload_digest || response.payload_length != request.payload_length || response.offset != request.offset) {
        handle_protocol_error(reconciler, &response, now);
        return;
    }
    memcpy(reconciler->outbound_chunk, response.chunk, response.chunk_length);
    reconciler->outbound_chunk_length = response.chunk_length;
    reconciler->state                 = NOAH_PROFILE_SPLIT_RECONCILER_PULL_WRITE;
    note_progress(reconciler, now);
}

static void pull_write(noah_profile_split_reconciler_t *reconciler) {
    noah_profile_peer_store_result_t result = noah_profile_peer_store_backend_write(reconciler->config.peer_store, reconciler->transfer_descriptor.generation, reconciler->transfer_descriptor.payload_digest, reconciler->transfer_offset, reconciler->outbound_chunk, reconciler->outbound_chunk_length);

    if (result != NOAH_PROFILE_PEER_STORE_OK) {
        stop_with_status(reconciler, map_peer_error(result));
        return;
    }
    reconciler->transfer_offset = noah_profile_peer_store_backend_next_offset(reconciler->config.peer_store);
    reconciler->state           = reconciler->transfer_offset == reconciler->transfer_descriptor.payload_length ? NOAH_PROFILE_SPLIT_RECONCILER_PULL_COMMIT_BEGIN : NOAH_PROFILE_SPLIT_RECONCILER_PULL_REQUEST;
    publish_authority(reconciler);
}

static void pull_commit_begin(noah_profile_split_reconciler_t *reconciler) {
    noah_profile_peer_store_result_t result = noah_profile_peer_store_backend_commit_begin(reconciler->config.peer_store, &reconciler->transfer_descriptor);

    if (result == NOAH_PROFILE_PEER_STORE_OK || result == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED) {
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
        reconciler->state          = NOAH_PROFILE_SPLIT_RECONCILER_VERIFY;
    } else if (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS) {
        reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PULL_COMMIT_STEP;
    } else {
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
        stop_with_status(reconciler, map_peer_error(result));
    }
    publish_authority(reconciler);
}

static void pull_commit_step(noah_profile_split_reconciler_t *reconciler) {
    noah_profile_peer_store_result_t result = noah_profile_peer_store_backend_step(reconciler->config.peer_store, NOAH_PROFILE_SPLIT_V1_CHUNK_MAX);

    if (result == NOAH_PROFILE_PEER_STORE_IN_PROGRESS) {
        return;
    }
    reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
    if (result == NOAH_PROFILE_PEER_STORE_OK || result == NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED) {
        refresh_local(reconciler);
        refresh_metadata_response(reconciler);
        reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_VERIFY;
    } else {
        stop_with_status(reconciler, map_peer_error(result));
    }
    publish_authority(reconciler);
}

static bool abort_transfer_for_role_change(noah_profile_split_reconciler_t *reconciler) {
    noah_profile_peer_store_state_t state = noah_profile_peer_store_backend_state(reconciler->config.peer_store);

    if (reconciler->transfer_owner == NOAH_PROFILE_SPLIT_TRANSFER_NONE) {
        return false;
    }
    if (state == NOAH_PROFILE_PEER_STORE_VALIDATING || state == NOAH_PROFILE_PEER_STORE_RECEIVING || state == NOAH_PROFILE_PEER_STORE_PREPARED) {
        (void)noah_profile_peer_store_backend_abort(reconciler->config.peer_store, &reconciler->config.peer_store->descriptor);
        reconciler->transfer_owner = NOAH_PROFILE_SPLIT_TRANSFER_NONE;
        return true;
    }
    return false;
}

void noah_profile_split_reconciler_init(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_reconciler_config_t *config) {
    if (!reconciler) {
        return;
    }
    memset(reconciler, 0, sizeof(*reconciler));
    if (config) {
        reconciler->config = *config;
    }
    noah_profile_split_authority_init(&reconciler->authority);
    reconciler->peer_descriptor = unreadable_descriptor();
    reconciler->retry_ms        = NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
    reconciler->state           = config && config->local_descriptor && config->local_read && config->exchange && config->peer_store ? NOAH_PROFILE_SPLIT_RECONCILER_PASSIVE : NOAH_PROFILE_SPLIT_RECONCILER_UNINITIALIZED;
    reconciler->initialized     = reconciler->state != NOAH_PROFILE_SPLIT_RECONCILER_UNINITIALIZED;
    if (reconciler->initialized) {
        refresh_local(reconciler);
        refresh_metadata_response(reconciler);
        publish_authority(reconciler);
    }
}

bool noah_profile_split_reconciler_scan_mode(noah_profile_split_reconciler_t *reconciler, bool master, uint32_t now_ms, noah_profile_split_reconcile_mode_t mode) {
    noah_profile_split_descriptor_t previous_local;
    bool                            losing_master;
    bool                            role_changed;

    if (!(reconciler && reconciler->initialized) || (mode != NOAH_PROFILE_SPLIT_RECONCILE_FULL && mode != NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY)) {
        return false;
    }
    previous_local = reconciler->local_descriptor;
    refresh_local(reconciler);
    if (!descriptor_equal(&previous_local, &reconciler->local_descriptor)) {
        refresh_metadata_response(reconciler);
        // Publish the durable local change before any retry/backoff return so
        // activation cannot observe an older converged authority snapshot.
        publish_authority(reconciler);
    }
    release_transient_busy_response(reconciler);
    if (reconciler->observed_peer_activity_sequence != reconciler->peer_activity_sequence) {
        reconciler->observed_peer_activity_sequence = reconciler->peer_activity_sequence;
        reconciler->peer_activity_known             = true;
        reconciler->last_peer_activity_at           = now_ms;
    }
    role_changed = reconciler->role_known && reconciler->master != master;
    losing_master = role_changed && reconciler->master && !master;
    if (!reconciler->role_known || role_changed) {
        if (losing_master && reconciler->prepared_push_active && reconciler->state >= NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN && reconciler->state <= NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT && reconciler->state != NOAH_PROFILE_SPLIT_RECONCILER_PUSH_ABORT) {
            // The opposite half may discard its pre-marker receiver lease as
            // it assumes master. Force an exact PREPARE_BEGIN/offset handshake
            // when this physical sender becomes master again.
            reconciler->transfer_offset       = 0u;
            reconciler->outbound_chunk_length = 0u;
            reconciler->state                 = reconciler->prepared_logical ? NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BIND : NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN;
        }
        reconciler->role_known      = true;
        reconciler->master          = master;
        if (!reconciler->prepared_push_active) {
            reconciler->state = master ? NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER : NOAH_PROFILE_SPLIT_RECONCILER_PASSIVE;
        }
        reconciler->next_attempt_at = now_ms;
        reconciler->retry_ms        = NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
        invalidate_peer(reconciler);
        if (abort_transfer_for_role_change(reconciler)) {
            publish_authority(reconciler);
            return true;
        }
    }
    if (advance_store_transfer(reconciler)) {
        return true;
    }
    if (mailbox_pending(reconciler)) {
        process_mailbox(reconciler, mode);
        return true;
    }
    if (expire_remote_prepare(reconciler, now_ms)) {
        return true;
    }
    if (!master) {
        if (!reconciler->prepared_push_active) {
            reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_PASSIVE;
        }
        if (reconciler->peer_activity_known && deadline_reached(now_ms, reconciler->last_peer_activity_at + NOAH_PROFILE_SPLIT_PEER_TIMEOUT_MS)) {
            reconciler->peer_activity_known = false;
            invalidate_peer(reconciler);
        }
        return false;
    }
    if (!reconciler->attempt_immediate && !deadline_reached(now_ms, reconciler->next_attempt_at)) {
        return false;
    }
    reconciler->attempt_immediate = false;
    if (mode == NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY && reconciler->state >= NOAH_PROFILE_SPLIT_RECONCILER_PULL_BEGIN && reconciler->state <= NOAH_PROFILE_SPLIT_RECONCILER_PULL_COMMIT_STEP) {
        reconciler->last_status = NOAH_PROFILE_SPLIT_V1_STATUS_BUSY;
        reconciler->state       = NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER;
        retry_later(reconciler, now_ms);
        publish_authority(reconciler);
        return true;
    }
    switch (reconciler->state) {
        case NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER:
        case NOAH_PROFILE_SPLIT_RECONCILER_VERIFY:
            metadata_exchange(reconciler, now_ms, mode);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BIND:
            push_bind(reconciler, now_ms);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN:
            push_begin(reconciler, now_ms);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_READ:
            push_read(reconciler);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_SEND:
            push_send(reconciler, now_ms);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_DURABLE:
            push_durable(reconciler, now_ms);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_PREPARED:
            publish_authority(reconciler);
            return false;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT:
            push_commit(reconciler, now_ms);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_ABORT:
            push_abort(reconciler, now_ms);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PULL_BEGIN:
            pull_begin(reconciler, now_ms);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PULL_BIND:
            pull_bind(reconciler, now_ms);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PULL_REQUEST:
            pull_request(reconciler, now_ms);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PULL_WRITE:
            pull_write(reconciler);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PULL_COMMIT_BEGIN:
            pull_commit_begin(reconciler);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_PULL_COMMIT_STEP:
            pull_commit_step(reconciler);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_CONVERGED:
            reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER;
            metadata_exchange(reconciler, now_ms, mode);
            return true;
        case NOAH_PROFILE_SPLIT_RECONCILER_STOPPED:
            if (!descriptor_equal(&reconciler->local_descriptor, &reconciler->stopped_local_descriptor)) {
                reconciler->state = NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER;
                note_progress(reconciler, now_ms);
                publish_authority(reconciler);
                return true;
            }
            return false;
        case NOAH_PROFILE_SPLIT_RECONCILER_PASSIVE:
        case NOAH_PROFILE_SPLIT_RECONCILER_UNINITIALIZED:
        default:
            return false;
    }
}

bool noah_profile_split_reconciler_scan(noah_profile_split_reconciler_t *reconciler, bool master, uint32_t now_ms) {
    return noah_profile_split_reconciler_scan_mode(reconciler, master, now_ms, NOAH_PROFILE_SPLIT_RECONCILE_FULL);
}

static bool prepared_push_begin(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor, void *source_context, noah_profile_split_local_read_fn source_read, bool logical, uint32_t via_generation, uint32_t via_digest) {
    if (!(reconciler && reconciler->initialized && descriptor && noah_profile_split_descriptor_valid(descriptor) && descriptor->readable && descriptor->has_profile && source_read)) {
        return false;
    }
    if (logical && (via_generation == 0u || via_digest == 0u)) {
        return false;
    }
    if (reconciler->prepared_push_active) {
        return descriptor_equal(&reconciler->transfer_descriptor, descriptor) && reconciler->prepared_logical == logical && reconciler->prepared_via_generation == via_generation && reconciler->prepared_via_digest == via_digest;
    }
    if (!(reconciler->role_known && reconciler->master && reconciler->transfer_owner == NOAH_PROFILE_SPLIT_TRANSFER_NONE) || (reconciler->state != NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER && reconciler->state != NOAH_PROFILE_SPLIT_RECONCILER_CONVERGED && reconciler->state != NOAH_PROFILE_SPLIT_RECONCILER_STOPPED)) {
        return false;
    }
    reconciler->transfer_descriptor     = *descriptor;
    reconciler->prepared_source_context = source_context;
    reconciler->prepared_source_read    = source_read;
    reconciler->transfer_offset         = 0u;
    reconciler->prepared_push_active    = true;
    reconciler->prepared_remote_started = false;
    reconciler->prepared_commit_authorized = false;
    reconciler->prepared_cancel_refused = false;
    reconciler->prepared_logical        = logical;
    reconciler->prepared_via_generation = via_generation;
    reconciler->prepared_via_digest     = via_digest;
    reconciler->last_status             = NOAH_PROFILE_SPLIT_V1_STATUS_OK;
    reconciler->attempt_immediate       = true;
    reconciler->retry_ms                = NOAH_PROFILE_SPLIT_RETRY_INITIAL_MS;
    reconciler->state                   = logical ? NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BIND : NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN;
    publish_authority(reconciler);
    return true;
}

bool noah_profile_split_reconciler_prepared_push_begin(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor, void *source_context, noah_profile_split_local_read_fn source_read) {
    return prepared_push_begin(reconciler, descriptor, source_context, source_read, false, 0u, 0u);
}

bool noah_profile_split_reconciler_prepared_push_begin_logical(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor, void *source_context, noah_profile_split_local_read_fn source_read, uint32_t via_generation, uint32_t via_digest) {
    return prepared_push_begin(reconciler, descriptor, source_context, source_read, true, via_generation, via_digest);
}

bool noah_profile_split_reconciler_prepared_push_ready(const noah_profile_split_reconciler_t *reconciler, noah_profile_split_descriptor_t *descriptor) {
    if (!(reconciler && reconciler->initialized && reconciler->prepared_push_active && reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_PREPARED)) {
        return false;
    }
    if (descriptor) {
        *descriptor = reconciler->transfer_descriptor;
    }
    return true;
}

bool noah_profile_split_reconciler_prepared_push_authorize_commit(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor) {
    if (!(reconciler && reconciler->initialized && descriptor && reconciler->prepared_push_active && descriptor_equal(&reconciler->transfer_descriptor, descriptor))) {
        return false;
    }
    if (reconciler->prepared_commit_authorized) {
        return reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BIND || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_READ || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_SEND || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_DURABLE || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_PREPARED || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_VERIFY || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_CONVERGED;
    }
    if (reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_PREPARED) {
        reconciler->prepared_commit_authorized = true;
        reconciler->state                      = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT;
        reconciler->attempt_immediate          = true;
        publish_authority(reconciler);
        return true;
    }
    return reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_VERIFY || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_CONVERGED;
}

bool noah_profile_split_reconciler_prepared_push_cancel(noah_profile_split_reconciler_t *reconciler, const noah_profile_split_descriptor_t *descriptor) {
    if (!(reconciler && reconciler->initialized && descriptor && descriptor_equal(&reconciler->transfer_descriptor, descriptor))) {
        return false;
    }
    if (reconciler->prepared_cancel_refused) {
        return false;
    }
    if (!reconciler->prepared_push_active) {
        return reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER;
    }
    if (reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_VERIFY || reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_CONVERGED) {
        return false;
    }
    if (reconciler->state == NOAH_PROFILE_SPLIT_RECONCILER_PUSH_ABORT) {
        return true;
    }
    if (!reconciler->prepared_remote_started) {
        clear_prepared_push(reconciler);
        reconciler->transfer_offset = 0u;
        reconciler->state           = NOAH_PROFILE_SPLIT_RECONCILER_DISCOVER;
    } else {
        reconciler->state             = NOAH_PROFILE_SPLIT_RECONCILER_PUSH_ABORT;
        reconciler->attempt_immediate = true;
    }
    publish_authority(reconciler);
    return true;
}

bool noah_profile_split_reconciler_provisional_peer_descriptor(const noah_profile_split_reconciler_t *reconciler, noah_profile_split_descriptor_t *descriptor) {
    if (!(reconciler && reconciler->initialized && descriptor && reconciler->provisional_peer_descriptor_known)) {
        return false;
    }
    *descriptor = reconciler->provisional_peer_descriptor;
    return true;
}

bool noah_profile_split_reconciler_refresh_authority(noah_profile_split_reconciler_t *reconciler) {
    if (!(reconciler && reconciler->initialized)) {
        return false;
    }
    refresh_local(reconciler);
    refresh_metadata_response(reconciler);
    publish_authority(reconciler);
    return true;
}

const noah_profile_split_authority_t *noah_profile_split_reconciler_authority(const noah_profile_split_reconciler_t *reconciler) {
    return reconciler && reconciler->initialized ? &reconciler->authority : NULL;
}

bool noah_profile_split_reconciler_status(const noah_profile_split_reconciler_t *reconciler, noah_profile_split_reconciler_status_t *status) {
    if (!(reconciler && reconciler->initialized && status)) {
        return false;
    }
    *status = (noah_profile_split_reconciler_status_t){
        .state                   = reconciler->state,
        .last_status             = reconciler->last_status,
        .transfer_offset         = reconciler->transfer_offset,
        .transfer_length         = reconciler->transfer_descriptor.payload_length,
        .transport_failure_count = reconciler->transport_failure_count,
        .retry_count             = reconciler->retry_count,
        .role_known              = reconciler->role_known,
        .master                  = reconciler->master,
        .mailbox_pending         = mailbox_pending(reconciler),
        .transfer_pending        = transfer_pending(reconciler),
    };
    return true;
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_profile_split_reconciler_t) <= NOAH_PROFILE_SPLIT_RECONCILER_STATE_BUDGET_32BIT, "profile split reconciler state exceeded its payload-independent firmware regression policy");
#endif

#endif
