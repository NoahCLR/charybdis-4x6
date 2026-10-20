// ───────────────────────────────────────────────────────────────────────────
// Working Copy Split Sync (D-F32)
// ───────────────────────────────────────────────────────────────────────────

#include "profile_working_sync.h"

#include <string.h>

#include "../storage/profile_checksum.h"

enum {
    FRAME_KIND        = 0u,
    FRAME_SEQUENCE    = 1u,
    FRAME_TRANSACTION = 3u,
    FRAME_SESSION     = 5u,
    FRAME_REVISION    = 9u,
    FRAME_BODY        = 13u,
    REPLY_FLAG        = 0x80u,
    // Reply fields.
    REPLY_RESULT       = 3u,
    REPLY_SESSION      = 4u,
    REPLY_REVISION     = 8u,
    REPLY_PHASE        = 12u,
    REPLY_LAST_RESULT  = 13u,
    REPLY_TRANSACTION  = 14u,
    REPLY_FLAGS        = 16u,
    REPLY_SAVED_GEN    = 17u,
    REPLY_SAVED_ORIGIN = 21u,
    REPLY_MODE_COUNTER = 22u,
    REPLY_MODE         = 26u,
    // A request the other half has queued is asked again no sooner than this:
    // it answers from its next scan.
    RETRY_MS = 2u,
};

static void put_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static void put_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static uint16_t get_u16(const uint8_t *source) {
    return (uint16_t)(source[0] | (uint16_t)source[1] << 8u);
}

static uint32_t get_u32(const uint8_t *source) {
    return (uint32_t)source[0] | (uint32_t)source[1] << 8u | (uint32_t)source[2] << 16u | (uint32_t)source[3] << 24u;
}

static bool elapsed_at_least(uint32_t now, uint32_t since, uint32_t interval) {
    return (uint32_t)(now - since) >= interval;
}

uint8_t noah_profile_working_sync_frame_length(const uint8_t *frame, uint8_t capacity) {
    uint8_t count;
    uint8_t length;

    if (!frame || capacity < NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE) {
        return 0u;
    }
    if ((frame[FRAME_KIND] & REPLY_FLAG) != 0u) {
        return (frame[FRAME_KIND] & (uint8_t)~REPLY_FLAG) != 0u && (frame[FRAME_KIND] & (uint8_t)~REPLY_FLAG) <= NOAH_PROFILE_WORKING_SYNC_KIND_MAX ? NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE : 0u;
    }
    if (frame[FRAME_KIND] == 0u || frame[FRAME_KIND] > NOAH_PROFILE_WORKING_SYNC_KIND_MAX) {
        return 0u;
    }
    if (frame[FRAME_KIND] != NOAH_PROFILE_WORKING_SYNC_DATA) {
        return NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE;
    }
    // A DATA frame is its bytes' size, and never shorter than any other frame.
    count = frame[FRAME_BODY + 4u];
    if (count == 0u || count > NOAH_PROFILE_WORKING_SYNC_DATA_MAX) {
        return 0u;
    }
    length = (uint8_t)(FRAME_BODY + 5u + count + 2u);
    if (length < NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE) {
        length = NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE;
    }
    return length <= capacity ? length : 0u;
}

static uint16_t frame_crc(const uint8_t *frame, uint8_t length) {
    return noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, frame, (size_t)length - 2u);
}

void noah_profile_working_sync_seal(uint8_t *frame, uint8_t length) {
    put_u16(&frame[length - 2u], frame_crc(frame, length));
}

static uint8_t frame_valid(const uint8_t *frame, uint8_t length) {
    uint8_t expected = noah_profile_working_sync_frame_length(frame, length);

    return expected != 0u && expected <= length && get_u16(&frame[expected - 2u]) == frame_crc(frame, expected) ? expected : 0u;
}

void noah_profile_working_sync_init(noah_profile_working_sync_t *sync, noah_profile_working_copy_t *copy, noah_profile_split_exchange_fn exchange, void *transport_context, const noah_profile_edit_mode_storage_t *mode_storage) {
    if (!sync) {
        return;
    }
    memset(sync, 0, sizeof(*sync));
    sync->copy              = copy;
    sync->exchange          = exchange;
    sync->transport_context = transport_context;
    sync->next_sequence     = 1u;
    sync->next_transaction  = 1u;
    sync->mode              = (noah_profile_edit_mode_record_t){.counter = 0u, .mode = NOAH_PROFILE_EDIT_MODE_DEFAULT};
    if (mode_storage) {
        sync->mode_storage = *mode_storage;
        if (!(sync->mode_storage.load && sync->mode_storage.load(sync->mode_storage.context, &sync->mode) && noah_profile_edit_mode_valid(sync->mode.mode))) {
            sync->mode = (noah_profile_edit_mode_record_t){.counter = 0u, .mode = NOAH_PROFILE_EDIT_MODE_DEFAULT};
        }
    }
}

// A new edit-mode record holds at once; it is written by store_mode.
static void queue_mode(noah_profile_working_sync_t *sync, noah_profile_edit_mode_record_t record) {
    sync->mode                  = record;
    sync->mode_store_pending    = true;
    sync->mode_store_failed     = false;
    sync->mode_store_backoff_ms = 0u;
}

bool noah_profile_working_sync_store_mode(noah_profile_working_sync_t *sync, uint32_t now_ms) {
    if (!sync || !sync->mode_store_pending || !sync->mode_storage.store) {
        return false;
    }
    if (sync->mode_store_failed && !elapsed_at_least(now_ms, sync->mode_store_failed_at, sync->mode_store_backoff_ms)) {
        return false;
    }
    if (sync->mode_storage.may_store && !sync->mode_storage.may_store(sync->mode_storage.context)) {
        return false;
    }
    if (sync->mode_storage.store(sync->mode_storage.context, &sync->mode)) {
        sync->mode_store_pending    = false;
        sync->mode_store_failed     = false;
        sync->mode_store_backoff_ms = 0u;
        return true;
    }
    // Not again on every poll: the wait doubles, and status reports it.
    sync->mode_store_failed     = true;
    sync->mode_store_failed_at  = now_ms;
    sync->mode_store_backoff_ms = sync->mode_store_backoff_ms == 0u ? NOAH_PROFILE_WORKING_SYNC_MODE_RETRY_MS : sync->mode_store_backoff_ms >= NOAH_PROFILE_WORKING_SYNC_MODE_RETRY_MAX_MS / 2u ? NOAH_PROFILE_WORKING_SYNC_MODE_RETRY_MAX_MS : sync->mode_store_backoff_ms * 2u;
    return true;
}

// What this half runs when nothing is edited: generation, origin, and
// whether that is the compiled defaults.
static bool saved_base(const noah_profile_working_copy_t *copy, uint32_t *generation, uint8_t *origin, bool *compiled) {
    noah_effective_profile_snapshot_t active;
    const noah_effective_profile_identity_t *identity;

    if (copy->active) {
        identity = &copy->saved.identity;
    } else if (noah_effective_profile_provider_copy_active(copy->provider, &active) == NOAH_EFFECTIVE_PROFILE_OK) {
        identity = &active.identity;
    } else {
        return false;
    }
    *generation = identity->generation;
    *origin     = identity->origin;
    *compiled   = identity->kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS || (identity->kind == NOAH_EFFECTIVE_PROFILE_KIND_WORKING_COPY && identity->generation == 0u);
    return true;
}

static void fill_status(const noah_profile_working_sync_t *sync, uint8_t *reply) {
    noah_profile_working_status_t status;
    uint32_t                      generation = 0u;
    uint8_t                       origin     = 0u;
    bool                          compiled   = false;

    if (!noah_profile_working_copy_status(sync->copy, &status)) {
        return;
    }
    put_u32(&reply[REPLY_SESSION], status.session);
    put_u32(&reply[REPLY_REVISION], status.revision);
    reply[REPLY_PHASE]       = status.phase;
    reply[REPLY_LAST_RESULT] = status.last_result;
    put_u16(&reply[REPLY_TRANSACTION], status.phase != NOAH_PROFILE_WORKING_PHASE_IDLE ? status.transaction_id : status.last_transaction_id);
    if (saved_base(sync->copy, &generation, &origin, &compiled)) {
        put_u32(&reply[REPLY_SAVED_GEN], generation);
        reply[REPLY_SAVED_ORIGIN] = origin;
    }
    reply[REPLY_FLAGS] = (uint8_t)((status.active ? NOAH_PROFILE_WORKING_SYNC_PEER_ACTIVE : 0u) | (compiled ? NOAH_PROFILE_WORKING_SYNC_PEER_SAVED_COMPILED : 0u) | (sync->stopped ? NOAH_PROFILE_WORKING_SYNC_PEER_STOPPED : 0u));
    put_u32(&reply[REPLY_MODE_COUNTER], sync->mode.counter);
    reply[REPLY_MODE] = sync->mode.mode;
}

// ── Other half ───────────────────────────────────────────────────────────────

static void short_reply(const uint8_t *request, uint8_t result, const uint8_t *status, uint8_t *response) {
    memset(response, 0, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE);
    if (status) {
        memcpy(response, status, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE);
    }
    response[FRAME_KIND] = (uint8_t)(request[FRAME_KIND] | REPLY_FLAG);
    memcpy(&response[FRAME_SEQUENCE], &request[FRAME_SEQUENCE], 2u);
    response[REPLY_RESULT] = result;
    noah_profile_working_sync_seal(response, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE);
}

bool noah_profile_working_sync_receive(noah_profile_working_sync_t *sync, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    uint8_t length;
    uint8_t observed;

    if (!sync || !request || !response || response_length < NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE) {
        return false;
    }
    length = frame_valid(request, request_length);
    if (length == 0u || (request[FRAME_KIND] & REPLY_FLAG) != 0u) {
        return false;
    }
    if (response_length > NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE) {
        memset(&response[NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE], 0, (size_t)response_length - NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE);
    }
    // The main loop never preempts this callback, so a settled reply stays
    // settled while it is copied.
    observed = noah_runtime_publication_observe(&sync->reply_sequence);
    if (noah_runtime_publication_in_flight(observed)) {
        short_reply(request, NOAH_PROFILE_WORKING_SYNC_BUSY, NULL, response);
        return true;
    }
    if (sync->reply[FRAME_KIND] == (uint8_t)(request[FRAME_KIND] | REPLY_FLAG) && memcmp(&sync->reply[FRAME_SEQUENCE], &request[FRAME_SEQUENCE], 2u) == 0) {
        memcpy(response, sync->reply, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE);
        return true;
    }
    if (sync->mailbox_full) {
        short_reply(request, NOAH_PROFILE_WORKING_SYNC_BUSY, sync->reply, response);
        return true;
    }
    memcpy(sync->mailbox, request, length);
    sync->mailbox_length = length;
    NOAH_RUNTIME_PUBLICATION_BARRIER();
    sync->mailbox_full = true;
    short_reply(request, NOAH_PROFILE_WORKING_SYNC_ADMITTED, sync->reply, response);
    return true;
}

static uint8_t process_request(noah_profile_working_sync_t *sync, const uint8_t *request) {
    noah_profile_working_copy_t *copy        = sync->copy;
    uint16_t                     transaction = get_u16(&request[FRAME_TRANSACTION]);
    uint32_t                     session     = get_u32(&request[FRAME_SESSION]);
    uint32_t                     revision    = get_u32(&request[FRAME_REVISION]);
    const uint8_t               *body        = &request[FRAME_BODY];

    // A stopped half says so and changes nothing (D-F34).
    if (sync->stopped) {
        return request[FRAME_KIND] == NOAH_PROFILE_WORKING_SYNC_STATUS ? NOAH_PROFILE_WORKING_OK : NOAH_PROFILE_WORKING_INVALID_STATE;
    }
    switch (request[FRAME_KIND]) {
        case NOAH_PROFILE_WORKING_SYNC_STATUS:
            return NOAH_PROFILE_WORKING_OK;
        case NOAH_PROFILE_WORKING_SYNC_BEGIN:
            return (uint8_t)noah_profile_working_copy_begin(copy, transaction, session, revision, body[0], get_u32(&body[2]), body[1], true);
        case NOAH_PROFILE_WORKING_SYNC_SPLICE: {
            noah_profile_working_splice_t splice = {.domain_id = body[1], .flags = body[2], .offset = get_u32(&body[3]), .remove = get_u32(&body[7]), .insert = get_u32(&body[11])};
            return (uint8_t)noah_profile_working_copy_splice(copy, transaction, body[0], &splice);
        }
        case NOAH_PROFILE_WORKING_SYNC_DATA:
            return (uint8_t)noah_profile_working_copy_data(copy, transaction, get_u32(&body[0]), &body[5], body[4]);
        case NOAH_PROFILE_WORKING_SYNC_COMMIT:
            return (uint8_t)noah_profile_working_copy_commit(copy, transaction, get_u32(&body[0]));
        case NOAH_PROFILE_WORKING_SYNC_ABORT:
            return (uint8_t)noah_profile_working_copy_abort(copy, transaction);
        case NOAH_PROFILE_WORKING_SYNC_REVERT:
            // The USB half's word: whatever this half was editing goes.
            if (copy->phase != NOAH_PROFILE_WORKING_PHASE_IDLE && copy->phase != NOAH_PROFILE_WORKING_PHASE_REVERTING) {
                (void)noah_profile_working_copy_abort(copy, copy->transaction_id);
            }
            return (uint8_t)noah_profile_working_copy_revert(copy, transaction, copy->session, copy->revision);
        case NOAH_PROFILE_WORKING_SYNC_ADOPT: {
            uint32_t generation;
            uint8_t  origin;
            bool     compiled;

            if (!saved_base(copy, &generation, &origin, &compiled) || generation != get_u32(&body[0]) || origin != body[4] || compiled != (body[5] != 0u)) {
                return NOAH_PROFILE_WORKING_INVALID_STATE;
            }
            return noah_profile_working_copy_adopt(copy, session, revision) ? NOAH_PROFILE_WORKING_OK : NOAH_PROFILE_WORKING_BUSY;
        }
        case NOAH_PROFILE_WORKING_SYNC_MODE: {
            noah_profile_edit_mode_record_t record = {.counter = get_u32(&body[0]), .mode = body[4]};

            if (!noah_profile_edit_mode_valid(record.mode)) {
                return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
            }
            if (record.counter < sync->mode.counter || (record.counter == sync->mode.counter && record.mode == sync->mode.mode)) {
                return NOAH_PROFILE_WORKING_OK;
            }
            if (!sync->mode_storage.store) {
                return NOAH_PROFILE_WORKING_READ_ERROR;
            }
            // Never written from here: the RPC answers at once, the write
            // waits for a quiet keyboard.
            queue_mode(sync, record);
            return NOAH_PROFILE_WORKING_OK;
        }
        default:
            return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
    }
}

bool noah_profile_working_sync_peer_scan(noah_profile_working_sync_t *sync) {
    uint8_t request[NOAH_PROFILE_WORKING_SYNC_FRAME_MAX];
    uint8_t reply[NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE];
    uint8_t result;

    if (!sync || !sync->mailbox_full) {
        return false;
    }
    NOAH_RUNTIME_PUBLICATION_BARRIER();
    memcpy(request, sync->mailbox, sync->mailbox_length);
    result = process_request(sync, request);
    memset(reply, 0, sizeof(reply));
    fill_status(sync, reply);
    reply[FRAME_KIND] = (uint8_t)(request[FRAME_KIND] | REPLY_FLAG);
    memcpy(&reply[FRAME_SEQUENCE], &request[FRAME_SEQUENCE], 2u);
    reply[REPLY_RESULT] = result;
    noah_profile_working_sync_seal(reply, sizeof(reply));
    noah_runtime_publication_begin(&sync->reply_sequence);
    memcpy(sync->reply, reply, sizeof(reply));
    noah_runtime_publication_end(&sync->reply_sequence);
    sync->mailbox_full = false;
    return true;
}

// ── USB half ─────────────────────────────────────────────────────────────────

static bool peer_matches(const noah_profile_working_sync_t *sync) {
    const noah_profile_working_peer_t *peer = &sync->peer;
    const noah_profile_working_copy_t *copy = sync->copy;

    return peer->known && peer->session == copy->session && peer->revision == copy->revision && peer->phase == NOAH_PROFILE_WORKING_PHASE_IDLE && ((peer->flags & NOAH_PROFILE_WORKING_SYNC_PEER_ACTIVE) != 0u) == copy->active && !(peer->flags & NOAH_PROFILE_WORKING_SYNC_PEER_STOPPED);
}

bool noah_profile_working_sync_converged(const noah_profile_working_sync_t *sync, uint32_t now_ms) {
    return sync && sync->copy && peer_matches(sync) && sync->forward_phase == NOAH_PROFILE_WORKING_FORWARD_NONE && !elapsed_at_least(now_ms, sync->peer.seen_at, NOAH_PROFILE_WORKING_SYNC_PEER_TIMEOUT_MS);
}

bool noah_profile_working_sync_set_mode(noah_profile_working_sync_t *sync, uint8_t mode) {
    noah_profile_edit_mode_record_t record;
    uint32_t                        counter;

    if (!sync || !noah_profile_edit_mode_valid(mode) || !sync->mode_storage.store) {
        return false;
    }
    counter = sync->mode.counter > sync->peer.mode_counter ? sync->mode.counter : sync->peer.mode_counter;
    if (counter == UINT32_MAX) {
        return false;
    }
    record = (noah_profile_edit_mode_record_t){.counter = counter + 1u, .mode = mode};
    queue_mode(sync, record);
    sync->mode_conflict = false;
    return true;
}

bool noah_profile_working_sync_refused(const noah_profile_working_sync_t *sync) {
    return sync && sync->copy && sync->copy->active && sync->forward_failed_whole && sync->forward_failed_revision == sync->copy->revision;
}

bool noah_profile_working_sync_peer_stopped(const noah_profile_working_sync_t *sync, uint32_t now_ms) {
    return sync && sync->peer.known && (sync->peer.flags & NOAH_PROFILE_WORKING_SYNC_PEER_STOPPED) && !elapsed_at_least(now_ms, sync->peer.seen_at, NOAH_PROFILE_WORKING_SYNC_PEER_TIMEOUT_MS);
}

static void begin_request(noah_profile_working_sync_t *sync, uint8_t kind, uint16_t transaction, uint32_t session, uint32_t revision) {
    memset(sync->request, 0, sizeof(sync->request));
    sync->request[FRAME_KIND] = kind;
    put_u16(&sync->request[FRAME_SEQUENCE], sync->next_sequence);
    sync->next_sequence = (uint16_t)(sync->next_sequence + 1u) ? (uint16_t)(sync->next_sequence + 1u) : 1u;
    put_u16(&sync->request[FRAME_TRANSACTION], transaction);
    put_u32(&sync->request[FRAME_SESSION], session);
    put_u32(&sync->request[FRAME_REVISION], revision);
    sync->request_length  = NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE;
    sync->request_pending = true;
}

static void seal_request(noah_profile_working_sync_t *sync) {
    noah_profile_working_sync_seal(sync->request, sync->request_length);
}

static void start_forward(noah_profile_working_sync_t *sync, bool replace) {
    const noah_profile_working_copy_t *copy = sync->copy;

    sync->forward_phase       = NOAH_PROFILE_WORKING_FORWARD_BEGIN;
    sync->forward_replace     = replace;
    sync->forward_revision    = copy->revision;
    sync->forward_index       = 0u;
    sync->forward_offset      = 0u;
    sync->forward_crc32       = NOAH_PROFILE_CRC32_INITIAL;
    sync->forward_total       = copy->published.insert_total;
    if (replace) {
        noah_profile_working_status_t status;

        sync->forward_total = noah_profile_working_copy_status(copy, &status) ? status.length : 0u;
    }
    sync->forward_transaction = sync->next_transaction;
    sync->next_transaction    = (uint16_t)(sync->next_transaction + 1u) ? (uint16_t)(sync->next_transaction + 1u) : 1u;
}

static bool forward_read(const noah_profile_working_sync_t *sync, uint32_t offset, uint8_t *bytes, uint32_t count) {
    return sync->forward_replace ? noah_profile_working_copy_read(sync->copy, offset, bytes, count) : noah_profile_working_copy_published_stream(sync->copy, offset, bytes, count);
}

static bool build_forward(noah_profile_working_sync_t *sync) {
    const noah_profile_working_copy_t *copy = sync->copy;
    uint32_t                           base = sync->forward_revision - 1u;

    switch (sync->forward_phase) {
        case NOAH_PROFILE_WORKING_FORWARD_BEGIN:
            begin_request(sync, NOAH_PROFILE_WORKING_SYNC_BEGIN, sync->forward_transaction, copy->session, base);
            sync->request[FRAME_BODY]      = sync->forward_replace ? 0u : copy->published.splice_count;
            sync->request[FRAME_BODY + 1u] = sync->forward_replace ? NOAH_PROFILE_WORKING_EDIT_REPLACE : copy->published.flags;
            put_u32(&sync->request[FRAME_BODY + 2u], sync->forward_total);
            break;
        case NOAH_PROFILE_WORKING_FORWARD_SPLICES: {
            const noah_profile_working_splice_t *splice = &copy->published.splices[sync->forward_index];

            begin_request(sync, NOAH_PROFILE_WORKING_SYNC_SPLICE, sync->forward_transaction, copy->session, base);
            sync->request[FRAME_BODY]      = sync->forward_index;
            sync->request[FRAME_BODY + 1u] = splice->domain_id;
            sync->request[FRAME_BODY + 2u] = splice->flags;
            put_u32(&sync->request[FRAME_BODY + 3u], splice->offset);
            put_u32(&sync->request[FRAME_BODY + 7u], splice->remove);
            put_u32(&sync->request[FRAME_BODY + 11u], splice->insert);
            break;
        }
        case NOAH_PROFILE_WORKING_FORWARD_DATA: {
            uint32_t count = sync->forward_total - sync->forward_offset;

            if (count > NOAH_PROFILE_WORKING_SYNC_DATA_MAX) {
                count = NOAH_PROFILE_WORKING_SYNC_DATA_MAX;
            }
            begin_request(sync, NOAH_PROFILE_WORKING_SYNC_DATA, sync->forward_transaction, copy->session, base);
            put_u32(&sync->request[FRAME_BODY], sync->forward_offset);
            sync->request[FRAME_BODY + 4u] = (uint8_t)count;
            if (!forward_read(sync, sync->forward_offset, &sync->request[FRAME_BODY + 5u], count)) {
                sync->request_pending = false;
                sync->forward_phase   = NOAH_PROFILE_WORKING_FORWARD_NONE;
                return false;
            }
            sync->request_length = noah_profile_working_sync_frame_length(sync->request, sizeof(sync->request));
            break;
        }
        case NOAH_PROFILE_WORKING_FORWARD_COMMIT:
            begin_request(sync, NOAH_PROFILE_WORKING_SYNC_COMMIT, sync->forward_transaction, copy->session, base);
            put_u32(&sync->request[FRAME_BODY], sync->forward_replace ? noah_profile_crc32_finish(sync->forward_crc32) : copy->published.insert_crc32);
            break;
        default:
            begin_request(sync, NOAH_PROFILE_WORKING_SYNC_STATUS, 0u, copy->session, copy->revision);
            break;
    }
    seal_request(sync);
    return true;
}

static void end_forward(noah_profile_working_sync_t *sync, bool failed) {
    if (failed) {
        sync->forward_failed_revision = sync->forward_revision;
        sync->forward_failed_whole    = sync->forward_replace;
    }
    sync->forward_phase = NOAH_PROFILE_WORKING_FORWARD_NONE;
}

static bool choose_request(noah_profile_working_sync_t *sync, uint32_t now) {
    const noah_profile_working_copy_t *copy = sync->copy;
    noah_profile_working_peer_t       *peer = &sync->peer;
    uint32_t                           generation;
    uint8_t                            origin;
    bool                               compiled;
    bool                               refused;

    if (sync->forward_phase != NOAH_PROFILE_WORKING_FORWARD_NONE && sync->forward_phase != NOAH_PROFILE_WORKING_FORWARD_WAIT) {
        // A Revert, an Apply or a newer profile replaced what was being
        // sent: the attempt ends, and the other half's unfinished edit is
        // aborted below once this half is idle.
        if (copy->revision == sync->forward_revision && (sync->forward_replace || (copy->published.valid && copy->published.revision == sync->forward_revision))) {
            return build_forward(sync);
        }
        end_forward(sync, false);
    }
    if (!peer->known || elapsed_at_least(now, peer->seen_at, sync->forward_phase == NOAH_PROFILE_WORKING_FORWARD_WAIT ? RETRY_MS : peer_matches(sync) || (peer->flags & NOAH_PROFILE_WORKING_SYNC_PEER_STOPPED) ? NOAH_PROFILE_WORKING_SYNC_IDLE_POLL_MS : NOAH_PROFILE_WORKING_SYNC_POLL_MS)) {
        if (!elapsed_at_least(now, sync->last_poll_at, RETRY_MS)) {
            return false;
        }
        begin_request(sync, NOAH_PROFILE_WORKING_SYNC_STATUS, 0u, copy->session, copy->revision);
        seal_request(sync);
        return true;
    }
    // A stopped other half takes nothing until it restarts: only polled.
    if (sync->forward_phase == NOAH_PROFILE_WORKING_FORWARD_WAIT || (peer->flags & NOAH_PROFILE_WORKING_SYNC_PEER_STOPPED)) {
        return false;
    }
    // The edit mode: the higher counter wins.
    if (peer->mode_counter > sync->mode.counter) {
        noah_profile_edit_mode_record_t record = {.counter = peer->mode_counter, .mode = peer->mode};

        if (noah_profile_edit_mode_valid(record.mode) && sync->mode_storage.store) {
            queue_mode(sync, record);
        }
    } else if (peer->mode_counter < sync->mode.counter) {
        begin_request(sync, NOAH_PROFILE_WORKING_SYNC_MODE, 0u, copy->session, copy->revision);
        put_u32(&sync->request[FRAME_BODY], sync->mode.counter);
        sync->request[FRAME_BODY + 4u] = sync->mode.mode;
        seal_request(sync);
        return true;
    }
    sync->mode_conflict = peer->mode_counter == sync->mode.counter && peer->mode != sync->mode.mode;
    if (peer_matches(sync) || copy->phase != NOAH_PROFILE_WORKING_PHASE_IDLE) {
        return false;
    }
    if (peer->phase == NOAH_PROFILE_WORKING_PHASE_RECEIVING) {
        begin_request(sync, NOAH_PROFILE_WORKING_SYNC_ABORT, peer->transaction_id, copy->session, copy->revision);
        seal_request(sync);
        return true;
    }
    if (peer->phase != NOAH_PROFILE_WORKING_PHASE_IDLE) {
        return false;
    }
    if (!saved_base(copy, &generation, &origin, &compiled)) {
        return false;
    }
    // Both halves must run the same saved profile first; the durable
    // reconciler sees to that.
    if (peer->saved_generation != generation || peer->saved_origin != origin || ((peer->flags & NOAH_PROFILE_WORKING_SYNC_PEER_SAVED_COMPILED) != 0u) != compiled) {
        return false;
    }
    // An edit the other half refused is followed by the whole working copy,
    // once: refused too, the halves stay apart until Revert or an Apply.
    refused = sync->forward_failed_revision == copy->revision;
    if (copy->active && peer->session == copy->session && peer->revision + 1u == copy->revision && !(refused && sync->forward_failed_whole)) {
        bool replace = refused || !(copy->published.valid && copy->published.revision == copy->revision && sync->agreed && sync->agreed_session == copy->session && sync->agreed_revision == peer->revision);

        // A replacement needs the other half on its saved profile.
        if (!replace || !(peer->flags & NOAH_PROFILE_WORKING_SYNC_PEER_ACTIVE)) {
            start_forward(sync, replace);
            return build_forward(sync);
        }
    }
    // REVERT and ADOPT reset the other half's bytes to its saved profile:
    // what was agreed before no longer describes them.
    if (peer->flags & NOAH_PROFILE_WORKING_SYNC_PEER_ACTIVE) {
        begin_request(sync, NOAH_PROFILE_WORKING_SYNC_REVERT, sync->next_transaction, copy->session, copy->revision);
        sync->next_transaction = (uint16_t)(sync->next_transaction + 1u) ? (uint16_t)(sync->next_transaction + 1u) : 1u;
        sync->agreed           = false;
        seal_request(sync);
        return true;
    }
    // The other half is on the same saved profile: it takes this half's
    // identity, or the one just before it, ready for the whole working copy.
    if (copy->active && (copy->revision == 0u || (refused && sync->forward_failed_whole))) {
        return false;
    }
    begin_request(sync, NOAH_PROFILE_WORKING_SYNC_ADOPT, 0u, copy->session, copy->active ? copy->revision - 1u : copy->revision);
    sync->agreed = false;
    put_u32(&sync->request[FRAME_BODY], generation);
    sync->request[FRAME_BODY + 4u] = origin;
    sync->request[FRAME_BODY + 5u] = compiled ? 1u : 0u;
    seal_request(sync);
    return true;
}

static void read_peer(noah_profile_working_sync_t *sync, const uint8_t *reply, uint32_t now) {
    noah_profile_working_peer_t *peer = &sync->peer;

    peer->session          = get_u32(&reply[REPLY_SESSION]);
    peer->revision         = get_u32(&reply[REPLY_REVISION]);
    peer->phase            = reply[REPLY_PHASE];
    peer->last_result      = reply[REPLY_LAST_RESULT];
    peer->transaction_id   = get_u16(&reply[REPLY_TRANSACTION]);
    peer->flags            = reply[REPLY_FLAGS];
    peer->saved_generation = get_u32(&reply[REPLY_SAVED_GEN]);
    peer->saved_origin     = reply[REPLY_SAVED_ORIGIN];
    peer->mode_counter     = get_u32(&reply[REPLY_MODE_COUNTER]);
    peer->mode             = reply[REPLY_MODE];
    peer->seen_at          = now;
    peer->known            = true;
    if (peer_matches(sync)) {
        sync->agreed          = true;
        sync->agreed_session  = peer->session;
        sync->agreed_revision = peer->revision;
    }
}

static void handle_reply(noah_profile_working_sync_t *sync, uint8_t kind, uint8_t result) {
    const noah_profile_working_peer_t *peer = &sync->peer;

    if (sync->forward_phase == NOAH_PROFILE_WORKING_FORWARD_WAIT) {
        if (peer->phase == NOAH_PROFILE_WORKING_PHASE_IDLE && peer->transaction_id == sync->forward_transaction) {
            end_forward(sync, !(peer->last_result == NOAH_PROFILE_WORKING_OK && peer->session == sync->copy->session && peer->revision == sync->forward_revision));
        } else if (peer->phase == NOAH_PROFILE_WORKING_PHASE_IDLE) {
            // The other half no longer holds this edit at all.
            end_forward(sync, false);
        }
        return;
    }
    if (sync->forward_phase == NOAH_PROFILE_WORKING_FORWARD_NONE || kind == NOAH_PROFILE_WORKING_SYNC_STATUS) {
        return;
    }
    if (result == NOAH_PROFILE_WORKING_OK || (result == NOAH_PROFILE_WORKING_QUEUED && kind == NOAH_PROFILE_WORKING_SYNC_COMMIT)) {
        switch (sync->forward_phase) {
            case NOAH_PROFILE_WORKING_FORWARD_BEGIN:
                sync->forward_phase = sync->forward_replace || sync->copy->published.splice_count == 0u ? NOAH_PROFILE_WORKING_FORWARD_DATA : NOAH_PROFILE_WORKING_FORWARD_SPLICES;
                if (sync->forward_phase == NOAH_PROFILE_WORKING_FORWARD_DATA && sync->forward_total == 0u) {
                    sync->forward_phase = NOAH_PROFILE_WORKING_FORWARD_COMMIT;
                }
                break;
            case NOAH_PROFILE_WORKING_FORWARD_SPLICES:
                if (++sync->forward_index == sync->copy->published.splice_count) {
                    sync->forward_phase = sync->forward_total == 0u ? NOAH_PROFILE_WORKING_FORWARD_COMMIT : NOAH_PROFILE_WORKING_FORWARD_DATA;
                }
                break;
            case NOAH_PROFILE_WORKING_FORWARD_DATA: {
                uint8_t count = sync->request[FRAME_BODY + 4u];

                sync->forward_crc32 = noah_profile_crc32_update(sync->forward_crc32, &sync->request[FRAME_BODY + 5u], count);
                sync->forward_offset += count;
                if (sync->forward_offset == sync->forward_total) {
                    sync->forward_phase = NOAH_PROFILE_WORKING_FORWARD_COMMIT;
                }
                break;
            }
            case NOAH_PROFILE_WORKING_FORWARD_COMMIT:
                sync->forward_phase = NOAH_PROFILE_WORKING_FORWARD_WAIT;
                break;
            default:
                break;
        }
        return;
    }
    // A refusal that says the halves disagree ends this attempt; the next
    // status decides again. One that says the edit itself is bad stops it
    // being sent again: the whole working copy is tried once instead.
    end_forward(sync, !(result == NOAH_PROFILE_WORKING_STALE || result == NOAH_PROFILE_WORKING_BUSY || result == NOAH_PROFILE_WORKING_INVALID_STATE || result == NOAH_PROFILE_WORKING_WRONG_TRANSACTION || result == NOAH_PROFILE_WORKING_NOT_CLEAN));
}

bool noah_profile_working_sync_master_scan(noah_profile_working_sync_t *sync, uint32_t now_ms) {
    uint8_t reply[NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE];
    uint8_t kind;
    uint8_t result;

    if (!sync || !sync->copy || !sync->exchange) {
        return false;
    }
    if (sync->peer.known && elapsed_at_least(now_ms, sync->peer.seen_at, NOAH_PROFILE_WORKING_SYNC_PEER_TIMEOUT_MS)) {
        // It may have restarted meanwhile.
        sync->peer.known    = false;
        sync->agreed        = false;
        sync->forward_phase = NOAH_PROFILE_WORKING_FORWARD_NONE;
    }
    if (!sync->request_pending) {
        if (!choose_request(sync, now_ms)) {
            return false;
        }
    } else if (!elapsed_at_least(now_ms, sync->last_poll_at, RETRY_MS)) {
        return false;
    }
    kind = sync->request[FRAME_KIND];
    sync->last_poll_at = now_ms;
    sync->exchanges++;
    memset(reply, 0, sizeof(reply));
    if (!sync->exchange(sync->transport_context, sync->request, sync->request_length, reply, sizeof(reply)) || frame_valid(reply, sizeof(reply)) != NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE || reply[FRAME_KIND] != (uint8_t)(kind | REPLY_FLAG) || memcmp(&reply[FRAME_SEQUENCE], &sync->request[FRAME_SEQUENCE], 2u) != 0) {
        sync->failed_exchanges++;
        return true;
    }
    result = reply[REPLY_RESULT];
    if (result == NOAH_PROFILE_WORKING_SYNC_ADMITTED || result == NOAH_PROFILE_WORKING_SYNC_BUSY) {
        return true;
    }
    sync->request_pending = false;
    read_peer(sync, reply, now_ms);
    handle_reply(sync, kind, result);
    return true;
}
