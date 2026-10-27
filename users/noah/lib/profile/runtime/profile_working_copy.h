// ───────────────────────────────────────────────────────────────────────────
// Working Copy: Live Edits In RAM (D-F32)
// ───────────────────────────────────────────────────────────────────────────
//
// Each half keeps a copy of its active profile in one RAM buffer the size of a
// slot's payload. An edit names splices into domains (remove some bytes at a
// domain-relative offset, insert others), arrives in pieces, is checked domain
// by domain against the bytes it would produce, and is published in place at
// a safe boundary. Nothing here writes flash: Save is an ordinary candidate
// made of REUSE from this copy (profile_owner.c), and a replug boots the saved
// profile.
//
// While nothing is edited the provider runs the saved profile itself (a slot
// or the compiled defaults) and the buffer is unused. The first edit copies
// the saved profile into the buffer, so the buffer becomes the active profile
// ("active" below, which is what dirty means). Revert, Save and Apply each
// return the provider to a saved profile.
//
// Inserted bytes are staged at the top of the buffer, above both the current
// and the resulting profile, so a refused edit leaves every byte of the
// working copy as it was, and no second buffer is needed. An edit therefore
// fits when its inserted bytes plus the larger of the two profiles fit the
// buffer. Replacing the whole working copy (an import) streams straight into
// the buffer instead, so it needs the saved profile active.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "active_profile_provider.h"
#include "../schema/profile_validator_v1.h"

enum {
    NOAH_PROFILE_WORKING_MAX_SPLICES = 8u,
    // A blob header, a header and its untouched bytes per domain, and per
    // splice its inserted bytes and the untouched bytes after it.
    NOAH_PROFILE_WORKING_MAX_PIECES = 1u + 2u * NOAH_PROFILE_DOMAIN_REGISTRY_COUNT + 2u * NOAH_PROFILE_WORKING_MAX_SPLICES,
    NOAH_PROFILE_WORKING_LITERAL_BYTES = NOAH_PROFILE_BLOB_V1_HEADER_SIZE + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE * NOAH_PROFILE_DOMAIN_REGISTRY_COUNT,
    // Validation reads RAM, so one scan grant may take this many validator
    // steps (each at most 20 bytes): a full macro bank in about 28 scans.
    NOAH_PROFILE_WORKING_VALIDATION_STEPS = 64u,
    // Bytes of the saved profile copied into the buffer per scan grant when
    // the first edit loads it.
    NOAH_PROFILE_WORKING_LOAD_BYTES = 1024u,
    // ...read in pieces no larger than a saved slot's reader takes at once
    // (the store's I/O bound, NOAH_PROFILE_STORE_IO_CHUNK_MAX, asserted in
    // profile_working_copy.c).
    NOAH_PROFILE_WORKING_LOAD_PIECE = 32u,
};

enum {
    // The domain is absent: the splice inserts all of it (offset and removal
    // zero), with a header the firmware writes in canonical order.
    NOAH_PROFILE_WORKING_SPLICE_CREATE = 1u << 0,
    // The domain goes, header and all: offset zero, removal its whole length,
    // nothing inserted. A required domain cannot be dropped.
    NOAH_PROFILE_WORKING_SPLICE_DROP  = 1u << 1,
    NOAH_PROFILE_WORKING_SPLICE_FLAGS = NOAH_PROFILE_WORKING_SPLICE_CREATE | NOAH_PROFILE_WORKING_SPLICE_DROP,
};

enum {
    // The edit replaces the whole working copy: no splices, the inserted
    // bytes are the new profile. Needs the saved profile active.
    NOAH_PROFILE_WORKING_EDIT_REPLACE = 1u << 0,
    NOAH_PROFILE_WORKING_EDIT_FLAGS   = NOAH_PROFILE_WORKING_EDIT_REPLACE,
};

typedef struct {
    uint32_t offset;  // in the domain's payload
    uint32_t remove;  // bytes removed there
    uint32_t insert;  // bytes inserted there, taken in splice order from the stream
    uint8_t  domain_id;
    uint8_t  flags;
} noah_profile_working_splice_t;

typedef enum {
    NOAH_PROFILE_WORKING_PHASE_IDLE = 0u,
    NOAH_PROFILE_WORKING_PHASE_RECEIVING,
    NOAH_PROFILE_WORKING_PHASE_LOADING,
    NOAH_PROFILE_WORKING_PHASE_VALIDATING,
    NOAH_PROFILE_WORKING_PHASE_WAITING_SAFE,
    NOAH_PROFILE_WORKING_PHASE_REVERTING,
} noah_profile_working_phase_t;

typedef enum {
    NOAH_PROFILE_WORKING_OK = 0u,
    // Accepted; the outcome is the edit status's last result.
    NOAH_PROFILE_WORKING_QUEUED,
    // The session or revision named is not the current one.
    NOAH_PROFILE_WORKING_STALE,
    // Another edit, a revert, a save or an Apply holds the working copy.
    NOAH_PROFILE_WORKING_BUSY,
    NOAH_PROFILE_WORKING_INVALID_ARGUMENT,
    NOAH_PROFILE_WORKING_INVALID_STATE,
    NOAH_PROFILE_WORKING_WRONG_TRANSACTION,
    NOAH_PROFILE_WORKING_OUT_OF_ORDER,
    NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED,
    NOAH_PROFILE_WORKING_CHECKSUM_MISMATCH,
    NOAH_PROFILE_WORKING_UNSUPPORTED_DOMAIN,
    NOAH_PROFILE_WORKING_INVALID_SPLICE,
    // A domain the edit touches would be invalid; nothing changed.
    NOAH_PROFILE_WORKING_VALIDATION_REJECTED,
    // A replacement needs the saved profile active (Revert or Save first).
    NOAH_PROFILE_WORKING_NOT_CLEAN,
    NOAH_PROFILE_WORKING_READ_ERROR,
    // The edit was abandoned: aborted, or a save or Apply replaced the
    // profile it was made against.
    NOAH_PROFILE_WORKING_ABORTED,
    NOAH_PROFILE_WORKING_PUBLICATION_FAILED,
} noah_profile_working_result_t;

typedef enum {
    NOAH_PROFILE_WORKING_PIECE_SOURCE = 0u, // the current profile, by its offset
    NOAH_PROFILE_WORKING_PIECE_STAGED,      // the inserted stream, by its offset
    NOAH_PROFILE_WORKING_PIECE_LITERAL,     // the plan's own header bytes
} noah_profile_working_piece_kind_t;

typedef struct {
    uint32_t offset; // in the resulting profile
    uint32_t length;
    uint32_t source;
    uint8_t  kind;
} noah_profile_working_piece_t;

typedef struct {
    uint32_t payload_offset;
    uint32_t length;
    bool     present;
} noah_profile_working_domain_t;

// The last published edit, kept until the next one so the master half can send
// the same edit to the other half: its splices, and where its inserted bytes
// now sit in the working copy, in stream order. A replacement's stream is the
// whole working copy.
typedef struct {
    noah_profile_working_splice_t splices[NOAH_PROFILE_WORKING_MAX_SPLICES];
    uint32_t                      stream_offsets[NOAH_PROFILE_WORKING_MAX_SPLICES];
    uint32_t                      insert_total;
    uint32_t                      insert_crc32;
    uint32_t                      revision; // the revision it produced
    uint8_t                       splice_count;
    uint8_t                       flags;
    bool                          valid;
} noah_profile_working_edit_record_t;

typedef struct {
    // Configuration.
    uint8_t                                         *bytes;
    uint32_t                                         capacity;
    noah_profile_reader_t                            reader; // bytes[0, capacity)
    noah_active_profile_provider_t                  *provider;
    const noah_profile_validator_v1_compatibility_t *host_rules; // edits from the host
    const noah_profile_validator_v1_compatibility_t *other_half_rules; // edits from the other half

    // The working copy.
    noah_active_profile_snapshot_t saved; // what Revert returns to, while active
    uint32_t                          session;
    uint32_t                          revision;
    bool                              active;
    // Reverting because Save found the copy identical to `saved`: the same
    // revision, and the edit status keeps reporting the last edit.
    bool                              settling;
    // The provider's publication count after this copy's own last
    // publication: any other publication replaced the profile under it.
    uint32_t                          publication_count;

    // The edit in progress.
    const noah_profile_validator_v1_compatibility_t *rules;
    noah_active_profile_snapshot_t                   base; // the active snapshot the edit was made against
    noah_profile_working_splice_t                    splices[NOAH_PROFILE_WORKING_MAX_SPLICES];
    noah_profile_working_piece_t                     pieces[NOAH_PROFILE_WORKING_MAX_PIECES];
    noah_profile_working_domain_t                    old_domains[NOAH_PROFILE_DOMAIN_REGISTRY_COUNT];
    noah_profile_working_domain_t                    new_domains[NOAH_PROFILE_DOMAIN_REGISTRY_COUNT];
    uint8_t                                          literal[NOAH_PROFILE_WORKING_LITERAL_BYTES];
    noah_profile_validator_v1_t                      validator;
    noah_profile_validator_v1_profile_t              next;
    noah_profile_validator_v1_error_t                error;
    uint32_t                                         insert_total;
    uint32_t                                         insert_received;
    uint32_t                                         insert_crc32;
    uint32_t                                         old_length;
    uint32_t                                         new_length;
    uint32_t                                         load_offset;
    uint32_t                                         target_revision;
    uint16_t                                         transaction_id;
    uint8_t                                          phase;
    uint8_t                                          flags;
    uint8_t                                          splice_count;
    uint8_t                                          splices_received;
    uint8_t                                          piece_count;
    uint8_t                                          validate_mask; // domains still to validate
    uint8_t                                          changed_mask;
    uint8_t                                          new_mask;

    noah_profile_working_edit_record_t published;

    // The last finished edit or revert.
    uint16_t last_transaction_id;
    uint8_t  last_result;
    uint8_t  last_phase; // the phase it ended in
} noah_profile_working_copy_t;

typedef struct {
    uint32_t session;
    uint32_t revision;
    uint32_t length;          // of the profile now active
    uint32_t staged_bytes;    // of the edit in progress
    uint16_t transaction_id;  // of the edit in progress
    uint16_t last_transaction_id;
    uint8_t  phase;
    uint8_t  last_result;
    uint8_t  splice_count;
    uint8_t  splices_received;
    uint8_t  domain_mask; // of the profile now active
    bool     active; // unsaved edits: the working copy is the active profile
    noah_profile_validator_v1_error_t error; // of the last refused edit
} noah_profile_working_status_t;

void noah_profile_working_copy_init(noah_profile_working_copy_t *copy, uint8_t *bytes, uint32_t capacity, noah_active_profile_provider_t *provider, const noah_profile_validator_v1_compatibility_t *host_rules, const noah_profile_validator_v1_compatibility_t *other_half_rules, uint32_t session);

// Takes `session` and `revision` as its own while no edit is in progress and
// the saved profile is active. The other half adopts the master half's identity
// this way when both run the same saved profile.
bool noah_profile_working_copy_adopt(noah_profile_working_copy_t *copy, uint32_t session, uint32_t revision);

// Admission, called where the edit arrives; each is bounded and copies at
// most the bytes it is given. `from_other_half` holds the edit to the other half's
// rules rather than a host's.
noah_profile_working_result_t noah_profile_working_copy_begin(noah_profile_working_copy_t *copy, uint16_t transaction_id, uint32_t session, uint32_t revision, uint8_t splice_count, uint32_t insert_total, uint8_t flags, bool from_other_half);
noah_profile_working_result_t noah_profile_working_copy_splice(noah_profile_working_copy_t *copy, uint16_t transaction_id, uint8_t index, const noah_profile_working_splice_t *splice);
noah_profile_working_result_t noah_profile_working_copy_data(noah_profile_working_copy_t *copy, uint16_t transaction_id, uint32_t offset, const uint8_t *bytes, uint32_t count);
// Every splice and inserted byte received, `crc32` over the inserted stream:
// the edit loads, validates and publishes over later scans (QUEUED).
noah_profile_working_result_t noah_profile_working_copy_commit(noah_profile_working_copy_t *copy, uint16_t transaction_id, uint32_t crc32);
noah_profile_working_result_t noah_profile_working_copy_abort(noah_profile_working_copy_t *copy, uint16_t transaction_id);
// Discards the working copy at the next safe boundary (QUEUED), or does
// nothing when the saved profile is already active (OK).
noah_profile_working_result_t noah_profile_working_copy_revert(noah_profile_working_copy_t *copy, uint16_t transaction_id, uint32_t session, uint32_t revision);
// For a Save that found the working copy byte for byte the saved profile it
// began from: the keyboard runs that saved profile again at the next safe
// boundary (QUEUED), with nothing unsaved and the same revision, since
// nothing it runs changes. OK with no working copy; busy during an edit.
noah_profile_working_result_t noah_profile_working_copy_settle_saved(noah_profile_working_copy_t *copy);

// One scan grant: a load chunk, a run of validator steps, or one attempt to
// publish. True when it did work.
bool noah_profile_working_copy_scan(noah_profile_working_copy_t *copy);

// The provider's active profile changed outside the working copy (a save or
// an Apply activated, or the other half's newer profile did): any edit in
// progress is abandoned and the working copy is no longer active. Returns
// true when it was active. `content_changed` advances the revision.
bool noah_profile_working_copy_note_replaced(noah_profile_working_copy_t *copy, bool content_changed);

// Notices a publication not its own (a save or an Apply activating, the
// other half's newer profile, boot) and applies note_replaced. `same_content`
// says the new profile is this working copy's own content (its save).
bool noah_profile_working_copy_observe(noah_profile_working_copy_t *copy, bool same_content);

// Reads `count` bytes of the last published edit's inserted stream from
// `offset`, as they sit in the working copy now.
bool noah_profile_working_copy_published_stream(const noah_profile_working_copy_t *copy, uint32_t offset, uint8_t *bytes, uint32_t count);

// Reads the profile now active: the working copy, or the saved profile.
bool noah_profile_working_copy_read(const noah_profile_working_copy_t *copy, uint32_t offset, uint8_t *bytes, uint32_t length);
bool noah_profile_working_copy_status(const noah_profile_working_copy_t *copy, noah_profile_working_status_t *status);
bool noah_profile_working_copy_busy(const noah_profile_working_copy_t *copy);
