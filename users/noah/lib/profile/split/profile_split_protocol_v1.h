// ──────────────────────────────────────────────────────────────────────────
// Live-Profile Split Protocol v1
// ─────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "profile_split_authority.h"

enum {
    NOAH_PROFILE_SPLIT_V1_PROTOCOL_VERSION = 1u,
    NOAH_PROFILE_SPLIT_V1_FRAME_SIZE       = 32u,
    NOAH_PROFILE_SPLIT_V1_CHUNK_MAX        = 14u,
};

typedef enum {
    NOAH_PROFILE_SPLIT_V1_METADATA       = 1u,
    NOAH_PROFILE_SPLIT_V1_PREPARE_BEGIN  = 2u,
    NOAH_PROFILE_SPLIT_V1_PAYLOAD_CHUNK  = 3u,
    NOAH_PROFILE_SPLIT_V1_PREPARE_COMMIT = 4u,
    NOAH_PROFILE_SPLIT_V1_ABORT          = 5u,
    NOAH_PROFILE_SPLIT_V1_ACK            = 6u,
    NOAH_PROFILE_SPLIT_V1_ERROR          = 7u,
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
} noah_profile_split_v1_status_t;

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
} noah_profile_split_v1_frame_t;

bool noah_profile_split_v1_frame_encode(const noah_profile_split_v1_frame_t *frame, uint8_t out[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]);
bool noah_profile_split_v1_frame_decode(const uint8_t *wire, uint8_t length, noah_profile_split_v1_frame_t *frame);
