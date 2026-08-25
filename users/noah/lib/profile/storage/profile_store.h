// ───────────────────────────────────────────────────────────────────────────
// Dual-Slot Persistent Live Profile Store
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "profile_storage_layout.h"

enum {
    NOAH_PROFILE_STORE_FORMAT_VERSION  = 1u,
    NOAH_PROFILE_STORE_SCHEMA_MAJOR    = 1u,
    NOAH_PROFILE_STORE_SCHEMA_MINOR    = 0u,
    NOAH_PROFILE_STORE_IO_CHUNK_MAX    = 32u,
    NOAH_PROFILE_STORE_FLAG_OVERRIDE   = 1u << 0,
    NOAH_PROFILE_STORE_ALLOWED_FLAGS   = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
};

typedef enum {
    NOAH_PROFILE_SLOT_NONE = 0u,
    NOAH_PROFILE_SLOT_A    = 1u,
    NOAH_PROFILE_SLOT_B    = 2u,
} noah_profile_slot_t;

typedef enum {
    NOAH_PROFILE_STORE_OK = 0u,
    NOAH_PROFILE_STORE_INVALID_ARGUMENT,
    NOAH_PROFILE_STORE_IO_ERROR,
    NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE,
    NOAH_PROFILE_STORE_INVALID_HEADER,
    NOAH_PROFILE_STORE_INCOMPATIBLE_SCHEMA,
    NOAH_PROFILE_STORE_INCOMPATIBLE_ACTION_ABI,
    NOAH_PROFILE_STORE_INVALID_PAYLOAD,
    NOAH_PROFILE_STORE_CHECKSUM_MISMATCH,
    NOAH_PROFILE_STORE_GENERATION_NOT_NEWER,
    NOAH_PROFILE_STORE_GENERATION_CONFLICT,
    NOAH_PROFILE_STORE_PREPARE_IN_PROGRESS,
    NOAH_PROFILE_STORE_NO_PREPARE,
    NOAH_PROFILE_STORE_CHUNK_OUT_OF_ORDER,
    NOAH_PROFILE_STORE_CHUNK_TOO_LARGE,
    NOAH_PROFILE_STORE_PAYLOAD_INCOMPLETE,
} noah_profile_store_result_t;

typedef bool (*noah_profile_store_read_fn)(void *context, uint16_t address, uint8_t *target, uint16_t length);
typedef bool (*noah_profile_store_write_fn)(void *context, uint16_t address, const uint8_t *source, uint16_t length);

typedef struct {
    noah_profile_store_read_fn  read;
    noah_profile_store_write_fn write;
    void                       *context;
} noah_profile_store_io_t;

typedef struct {
    uint8_t  schema_major;
    uint8_t  schema_minor;
    uint32_t action_abi_digest;
} noah_profile_store_compatibility_t;

typedef struct {
    noah_profile_slot_t slot;
    uint8_t             schema_major;
    uint8_t             schema_minor;
    uint8_t             flags;
    uint16_t            payload_length;
    uint32_t            generation;
    uint8_t             origin_half;
    uint32_t            payload_crc32;
    uint32_t            payload_digest;
    uint32_t            compiled_default_digest;
    uint32_t            action_abi_digest;
} noah_profile_store_record_t;

typedef struct {
    uint8_t  schema_major;
    uint8_t  schema_minor;
    uint8_t  flags;
    uint16_t payload_length;
    uint32_t generation;
    uint8_t  origin_half;
    uint32_t payload_crc32;
    uint32_t payload_digest;
    uint32_t compiled_default_digest;
    uint32_t action_abi_digest;
} noah_profile_store_candidate_t;

typedef struct {
    noah_profile_store_io_t            io;
    noah_profile_store_compatibility_t compatibility;
    noah_profile_store_record_t        committed;
    noah_profile_store_candidate_t     candidate;
    noah_profile_slot_t                candidate_slot;
    uint16_t                           candidate_written;
    uint32_t                           candidate_crc32_state;
    uint32_t                           candidate_digest_state;
    bool                               boot_scanned;
    bool                               conflict;
    bool                               prepare_active;
    uint8_t                            scratch[NOAH_PROFILE_STORE_IO_CHUNK_MAX];
} noah_profile_store_t;

void noah_profile_store_init(noah_profile_store_t *store, noah_profile_store_io_t io, noah_profile_store_compatibility_t compatibility);
noah_profile_store_result_t noah_profile_store_boot_select(noah_profile_store_t *store, noah_profile_store_record_t *selected);
noah_profile_store_result_t noah_profile_store_validate_slot(noah_profile_store_t *store, noah_profile_slot_t slot, bool require_commit, noah_profile_store_record_t *record);
noah_profile_store_result_t noah_profile_store_prepare_begin(noah_profile_store_t *store, const noah_profile_store_candidate_t *candidate);
noah_profile_store_result_t noah_profile_store_prepare_write(noah_profile_store_t *store, uint16_t offset, const uint8_t *bytes, uint16_t length);
noah_profile_store_result_t noah_profile_store_prepare_commit(noah_profile_store_t *store, noah_profile_store_record_t *committed);
noah_profile_store_result_t noah_profile_store_prepare_abort(noah_profile_store_t *store);
bool noah_profile_store_next_generation(const noah_profile_store_t *store, uint32_t *generation);
