// ──────────────────────────────────────────────────────────────────────────
// Live-Profile Split Protocol v1
// ─────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "profile_split_authority.h"

enum {
    // Framing version 2 added sized payload chunks and PAYLOAD_REUSE. Both
    // halves run one build, so an older peer simply fails to decode.
    NOAH_PROFILE_SPLIT_PROTOCOL_VERSION = 2u,
    // Every reply, and every request except the two below, is this size.
    NOAH_PROFILE_SPLIT_V1_FRAME_SIZE = 32u,
    // The largest request: a full payload chunk. It sizes QMK's master-to-
    // slave RPC buffer; replies keep the 32-byte slave-to-master buffer.
    NOAH_PROFILE_SPLIT_V1_FRAME_MAX = 128u,
    // A payload chunk frame is its 17-byte header, the chunk and the CRC.
    NOAH_PROFILE_SPLIT_V1_CHUNK_OVERHEAD = 18u,
    NOAH_PROFILE_SPLIT_V1_CHUNK_MAX      = NOAH_PROFILE_SPLIT_V1_FRAME_MAX - NOAH_PROFILE_SPLIT_V1_CHUNK_OVERHEAD,
    // A chunk that answers a PAYLOAD_REQUEST travels in a 32-byte reply.
    NOAH_PROFILE_SPLIT_V1_REPLY_CHUNK_MAX  = NOAH_PROFILE_SPLIT_V1_FRAME_SIZE - NOAH_PROFILE_SPLIT_V1_CHUNK_OVERHEAD,
    NOAH_PROFILE_SPLIT_V1_REUSE_FRAME_SIZE = 36u,
};

typedef enum {
    NOAH_PROFILE_SPLIT_V1_METADATA       = 1u,
    NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN  = 2u,
    NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK  = 3u,
    NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT = 4u,
    NOAH_PROFILE_SPLIT_V1_ABORT          = 5u,
    NOAH_PROFILE_SPLIT_V1_ACK            = 6u,
    NOAH_PROFILE_SPLIT_V1_ERROR          = 7u,
    // The current QMK master is the only RPC initiator. This request lets it
    // pull a newer durable profile from the sibling without making USB role
    // part of authority. The response is a correlated PAYLOAD_CHUNK.
    NOAH_PROFILE_SPLIT_V1_PAYLOAD_REQUEST = 8u,
    // Carries the VIA identity bound to a format-3 custom record. It is sent
    // before PREPARE_BEGIN so the peer can write the exact same slot header.
    NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND = 9u,
    // Validates the received payload and persists only the prepared marker.
    // PREPARE_COMMIT remains the later logical decision command.
    NOAH_PROFILE_SPLIT_V1_PREPARE_DURABLE      = 10u,
    NOAH_PROFILE_SPLIT_V1_LOGICAL_BIND_REQUEST = 11u,
    // Asks the receiver to fill a range of the copy from its own active
    // profile, which it checks against the named source identity. It answers
    // like a payload chunk; SOURCE_UNAVAILABLE means "send these bytes".
    NOAH_PROFILE_SPLIT_V1_PAYLOAD_REUSE = 12u,
} noah_profile_split_v1_kind_t;

typedef enum {
    NOAH_PROFILE_SPLIT_V1_STATUS_OK = 0u,
    NOAH_PROFILE_SPLIT_V1_STATUS_INVALID_FRAME,
    NOAH_PROFILE_SPLIT_V1_STATUS_INCOMPATIBLE,
    NOAH_PROFILE_SPLIT_V1_STATUS_STALE,
    NOAH_PROFILE_SPLIT_V1_STATUS_CONFLICT,
    NOAH_PROFILE_SPLIT_V1_STATUS_CORRUPT,
    NOAH_PROFILE_SPLIT_V1_STATUS_BUSY,
    NOAH_PROFILE_SPLIT_V1_STATUS_RANGE_ERROR,
    NOAH_PROFILE_SPLIT_V1_STATUS_DIGEST_MISMATCH,
    NOAH_PROFILE_SPLIT_V1_STATUS_STORAGE_ERROR,
    NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR,
    // PAYLOAD_REUSE only: the receiver's active profile is not the named
    // source. Its offset is how far the copy got; the lease stays open.
    NOAH_PROFILE_SPLIT_V1_STATUS_SOURCE_UNAVAILABLE,
} noah_profile_split_v1_status_t;

// Why a receiver answered ACK/BUSY, carried in that reply's unused chunk
// bytes so the sender can report it. Diagnostic only: no decision depends on
// it, and 0 means the receiver did not say.
typedef enum {
    NOAH_PROFILE_SPLIT_V1_BUSY_UNSPECIFIED      = 0u,
    NOAH_PROFILE_SPLIT_V1_BUSY_ADMITTED         = 1u, // queued; the answer comes on a retry
    NOAH_PROFILE_SPLIT_V1_BUSY_MAILBOX_FULL     = 2u, // an earlier request is still unprocessed
    NOAH_PROFILE_SPLIT_V1_BUSY_OTHER_COPY       = 3u, // the store holds a different copy
    NOAH_PROFILE_SPLIT_V1_BUSY_NO_LEASE         = 4u, // the store is not receiving this copy
    NOAH_PROFILE_SPLIT_V1_BUSY_STORE_WORKING    = 5u, // validating, preparing or committing
    NOAH_PROFILE_SPLIT_V1_BUSY_PULLING          = 6u, // the receiver is pulling a profile itself
    NOAH_PROFILE_SPLIT_V1_BUSY_CONVERGENCE_ONLY = 7u, // the receiver only converges just now
    NOAH_PROFILE_SPLIT_V1_BUSY_COPYING          = 8u, // copying a PAYLOAD_REUSE range; offset is its progress
    NOAH_PROFILE_SPLIT_V1_BUSY_REASON_MAX       = NOAH_PROFILE_SPLIT_V1_BUSY_COPYING,
} noah_profile_split_v1_busy_reason_t;

// The active profile a PAYLOAD_REUSE range is read from, as Profile Wire's
// candidate REUSE names it: compiled (kind 0, generation 0, origin 255) or
// committed (kind 1, nonzero generation, origin 0..1).
typedef struct {
    uint32_t generation;
    uint32_t digest;
    uint32_t crc32;
    uint8_t  kind;
    uint8_t  origin;
} noah_profile_split_v1_source_t;

typedef struct {
    noah_profile_split_v1_kind_t    kind;
    noah_profile_split_v1_status_t  status;
    noah_profile_split_descriptor_t descriptor;
    uint32_t                        generation;
    uint32_t                        payload_digest;
    uint16_t                        offset;
    uint16_t                        payload_length;
    uint8_t                         chunk_length;
    uint8_t                         chunk[NOAH_PROFILE_SPLIT_V1_CHUNK_MAX];
    // PAYLOAD_REUSE only: fill `reuse_length` bytes at `offset` from the
    // source's bytes at `reuse_source_offset`.
    uint16_t                        reuse_length;
    uint16_t                        reuse_source_offset;
    noah_profile_split_v1_source_t  reuse_source;
    uint8_t                         store_format_version;
    uint32_t                        via_generation;
    uint32_t                        via_digest;
    // ACK/BUSY only; zero in every other frame.
    uint8_t                         busy_reason;
    uint8_t                         busy_store_state;
    uint8_t                         busy_owner;
    uint8_t                         busy_admission; // noah_profile_storage_admission_owner_t
} noah_profile_split_v1_frame_t;

// The frame's length on the wire: 32 bytes, a payload chunk's header plus
// its chunk, or a reuse frame's 36. Zero for an invalid frame.
uint8_t noah_profile_split_v1_frame_length(const noah_profile_split_v1_frame_t *frame);
// Returns the encoded length, or zero when the frame is invalid or does not
// fit `capacity`.
uint8_t noah_profile_split_v1_frame_encode(const noah_profile_split_v1_frame_t *frame, uint8_t *out, uint8_t capacity);
// Accepts exactly one frame of its kind's length.
bool noah_profile_split_v1_frame_decode(const uint8_t *wire, uint8_t length, noah_profile_split_v1_frame_t *frame);
