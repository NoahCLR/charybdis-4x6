// ──────────────────────────────────────────────────────────────────────────
// Exact Peer Profile -> Shared Candidate/Store Backend
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../split/profile_split_protocol_v1.h"
#include "profile_candidate_store_backend.h"
#include "profile_record_identity.h"

enum {
    // Firmware-state regression policy, not an RP2040 SRAM-capacity claim.
    NOAH_PROFILE_PEER_STORE_STATE_BUDGET_32BIT = 256u,
};

typedef enum {
    NOAH_PROFILE_PEER_STORE_UNINITIALIZED = 0u,
    NOAH_PROFILE_PEER_STORE_IDLE,
    NOAH_PROFILE_PEER_STORE_RECEIVING,
    NOAH_PROFILE_PEER_STORE_VALIDATING,
    NOAH_PROFILE_PEER_STORE_PREPARING,
    NOAH_PROFILE_PEER_STORE_PREPARED,
    NOAH_PROFILE_PEER_STORE_COMMITTING,
    NOAH_PROFILE_PEER_STORE_COMMITTED,
    NOAH_PROFILE_PEER_STORE_REJECTED,
    NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED,
} noah_profile_peer_store_state_t;

typedef enum {
    NOAH_PROFILE_PEER_STORE_OK = 0u,
    NOAH_PROFILE_PEER_STORE_IN_PROGRESS,
    NOAH_PROFILE_PEER_STORE_ALREADY_COMMITTED,
    NOAH_PROFILE_PEER_STORE_BUSY,
    NOAH_PROFILE_PEER_STORE_STALE,
    NOAH_PROFILE_PEER_STORE_CONFLICT,
    NOAH_PROFILE_PEER_STORE_CORRUPT,
    NOAH_PROFILE_PEER_STORE_INCOMPATIBLE,
    NOAH_PROFILE_PEER_STORE_INVALID_METADATA,
    NOAH_PROFILE_PEER_STORE_RANGE_ERROR,
    NOAH_PROFILE_PEER_STORE_CHUNK_CONFLICT,
    NOAH_PROFILE_PEER_STORE_STORAGE_ERROR,
    NOAH_PROFILE_PEER_STORE_VALIDATION_ERROR,
    NOAH_PROFILE_PEER_STORE_DURABILITY_UNKNOWN,
    // A reuse source this half's active profile does not match. Not a
    // rejection: the copy stays open at its next offset for the bytes.
    NOAH_PROFILE_PEER_STORE_SOURCE_UNAVAILABLE,
} noah_profile_peer_store_result_t;

typedef struct {
    noah_profile_candidate_store_backend_t *backend;
    noah_profile_candidate_backend_t        interface;
    noah_profile_split_descriptor_t         descriptor;
    noah_profile_candidate_v1_metadata_t    metadata;
    noah_profile_candidate_v1_error_t       validation_error;
    noah_profile_peer_store_state_t         state;
    noah_profile_peer_store_result_t        result;
    uint32_t                                next_offset;
    bool                                    auto_commit;
} noah_profile_peer_store_backend_t;

// The peer receiver borrows the one candidate backend already shared with the
// host transaction. It never installs another store reuse guard or provider.
void noah_profile_peer_store_backend_init(noah_profile_peer_store_backend_t *peer, noah_profile_candidate_store_backend_t *backend);

// Opens exact sender-owned staging. Generation, physical origin, persistent
// flags, and all payload identities are preserved unchanged.
noah_profile_peer_store_result_t noah_profile_peer_store_backend_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor);

// Accepts only sequential chunks of up to one split chunk, stored in pieces
// the candidate store takes. A fully repeated chunk is idempotent when its
// staged bytes match; gaps, partial overlaps, and conflicting repeats poison
// and abort the prepare.
noah_profile_peer_store_result_t noah_profile_peer_store_backend_write(noah_profile_peer_store_backend_t *peer, uint32_t generation, uint32_t payload_digest, uint32_t offset, const uint8_t *bytes, uint8_t length);

// Fills the copy from this half's active profile for a PAYLOAD_REUSE range
// that starts at `offset` in the copy and at `source_offset` in `source`, and
// ends before `end`. It continues from the next offset (a range may start
// before it, never after), copying at most `byte_budget` bytes in pieces the
// candidate store takes, and checks the source's identity on every read. OK
// means progress; the range is done when the next offset reaches `end`.
noah_profile_peer_store_result_t noah_profile_peer_store_backend_reuse(noah_profile_peer_store_backend_t *peer, uint32_t generation, uint32_t payload_digest, uint32_t offset, const noah_profile_split_v1_source_t *source, uint32_t source_offset, uint32_t end, uint16_t byte_budget);

// Starts whole-profile validation after every declared byte arrived. step()
// performs at most one bounded validator/store operation per call.
noah_profile_peer_store_result_t noah_profile_peer_store_backend_commit_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor);
noah_profile_peer_store_result_t noah_profile_peer_store_backend_prepare_durable_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor);
noah_profile_peer_store_result_t noah_profile_peer_store_backend_prepared_commit_begin(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor);
noah_profile_peer_store_result_t noah_profile_peer_store_backend_step(noah_profile_peer_store_backend_t *peer, uint8_t byte_budget);

// Abort is allowed only before marker-last commit begins. Once durability can
// be ambiguous the caller must finish or reboot-scan the store.
noah_profile_peer_store_result_t noah_profile_peer_store_backend_abort(noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *descriptor);

noah_profile_peer_store_state_t          noah_profile_peer_store_backend_state(const noah_profile_peer_store_backend_t *peer);
noah_profile_peer_store_result_t         noah_profile_peer_store_backend_result(const noah_profile_peer_store_backend_t *peer);
uint32_t                                 noah_profile_peer_store_backend_next_offset(const noah_profile_peer_store_backend_t *peer);
const noah_profile_candidate_v1_error_t *noah_profile_peer_store_backend_validation_error(const noah_profile_peer_store_backend_t *peer);

// The peer store answers its own state questions; callers ask these rather
// than spell out sets of states. Static inline: the compiler may keep one out of line as a small leaf of
// its own; the reviewed stack paths are measured as built.

// An admitted copy is not finished: it is receiving, being validated,
// prepared, being made durable or committed, or waits for a boot scan to
// say whether its commit landed. Until it ends the store answers only for it.
static inline bool noah_profile_peer_store_backend_holds_copy(const noah_profile_peer_store_backend_t *peer) {
    noah_profile_peer_store_state_t state = noah_profile_peer_store_backend_state(peer);

    return state == NOAH_PROFILE_PEER_STORE_RECEIVING || state == NOAH_PROFILE_PEER_STORE_VALIDATING || state == NOAH_PROFILE_PEER_STORE_PREPARING || state == NOAH_PROFILE_PEER_STORE_PREPARED || state == NOAH_PROFILE_PEER_STORE_COMMITTING || state == NOAH_PROFILE_PEER_STORE_RECONCILE_REQUIRED;
}

// A bounded step is due: the store validates the copy, makes its prepared
// marker durable or commits it, one step per call to step().
static inline bool noah_profile_peer_store_backend_working(const noah_profile_peer_store_backend_t *peer) {
    noah_profile_peer_store_state_t state = noah_profile_peer_store_backend_state(peer);

    return state == NOAH_PROFILE_PEER_STORE_VALIDATING || state == NOAH_PROFILE_PEER_STORE_PREPARING || state == NOAH_PROFILE_PEER_STORE_COMMITTING;
}

// The copy can still be abandoned: no commit decision is durable yet (a
// PREPARED copy's prepared marker is), and abort() ends it.
static inline bool noah_profile_peer_store_backend_abortable(const noah_profile_peer_store_backend_t *peer) {
    noah_profile_peer_store_state_t state = noah_profile_peer_store_backend_state(peer);

    return state == NOAH_PROFILE_PEER_STORE_RECEIVING || state == NOAH_PROFILE_PEER_STORE_VALIDATING || state == NOAH_PROFILE_PEER_STORE_PREPARED;
}

// The copy is committed and this half's storage is still held for it, so it
// waits to be published. The store stays COMMITTED after publication (it
// answers a sender's retry of the finished copy), so its state alone is no
// sign of a copy to publish: a later Apply or Save holds the storage then.
static inline bool noah_profile_peer_store_backend_awaiting_activation(const noah_profile_peer_store_backend_t *peer) {
    return peer && noah_profile_peer_store_backend_state(peer) == NOAH_PROFILE_PEER_STORE_COMMITTED && noah_profile_candidate_store_backend_admission_owner(peer->backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER;
}

// The copy the store last admitted is this one, by generation, digest and
// length. Whether the store still holds it is its state's question.
static inline bool noah_profile_peer_store_backend_copy_matches(const noah_profile_peer_store_backend_t *peer, uint32_t generation, uint32_t payload_digest, uint32_t payload_length) {
    return peer && peer->descriptor.readable && peer->descriptor.has_profile && peer->descriptor.generation == generation && peer->descriptor.payload_digest == payload_digest && peer->descriptor.payload_length == payload_length;
}

// A prepared copy its sender has abandoned. `sender_saved` is the saved
// profile of a sender that holds no prepared push; if it is readable and is
// not the copy, the sender restarted before its marker or released the push
// without an acknowledged ABORT, and nothing will ever end the copy, so the
// caller releases it (D-F34). A sender whose saved profile is the copy
// commits it as repair. Nothing else abandons a prepared copy: not time, nor
// silence.
static inline bool noah_profile_peer_store_backend_prepared_copy_abandoned(const noah_profile_peer_store_backend_t *peer, const noah_profile_split_descriptor_t *sender_saved) {
    return peer && noah_profile_peer_store_backend_state(peer) == NOAH_PROFILE_PEER_STORE_PREPARED && sender_saved && sender_saved->readable && !noah_profile_descriptor_equal(sender_saved, &peer->descriptor);
}
