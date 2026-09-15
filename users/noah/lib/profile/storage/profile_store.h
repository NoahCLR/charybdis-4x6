// ───────────────────────────────────────────────────────────────────────────
// Dual-Slot Persistent Live Profile Store
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "profile_storage_layout.h"

enum {
    NOAH_PROFILE_STORE_FORMAT_VERSION_LEGACY  = 1u,
    NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL = 2u,
    NOAH_PROFILE_STORE_FORMAT_VERSION         = NOAH_PROFILE_STORE_FORMAT_VERSION_LOGICAL,
    NOAH_PROFILE_STORE_SCHEMA_MAJOR   = 1u,
    NOAH_PROFILE_STORE_SCHEMA_MINOR   = 0u,
    NOAH_PROFILE_STORE_IO_CHUNK_MAX   = 32u,
    NOAH_PROFILE_STORE_FLAG_OVERRIDE  = 1u << 0,
    NOAH_PROFILE_STORE_ALLOWED_FLAGS  = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
    // Regression policy for the persistent store's complete writable state
    // on the 32-bit RP2040 target. This is not a physical SRAM limit.
    NOAH_PROFILE_STORE_STATE_BUDGET_32BIT = 384u,
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
    NOAH_PROFILE_STORE_INCOMPATIBLE_COMPILED_DEFAULT,
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
    NOAH_PROFILE_STORE_BACKING_REUSE_DENIED,
    NOAH_PROFILE_STORE_BACKING_REUSE_RELEASE_FAILED,
    NOAH_PROFILE_STORE_IN_PROGRESS,
    // The final marker write may have become durable but could not be
    // confirmed. The caller must reconcile persistent status before retrying.
    NOAH_PROFILE_STORE_DURABILITY_UNKNOWN,
} noah_profile_store_result_t;

typedef enum {
    NOAH_PROFILE_STORE_COMMIT_IDLE = 0u,
    NOAH_PROFILE_STORE_COMMIT_HEADER_WRITE,
    NOAH_PROFILE_STORE_COMMIT_HEADER_READBACK,
    NOAH_PROFILE_STORE_COMMIT_PAYLOAD_READBACK,
    NOAH_PROFILE_STORE_COMMIT_SHAPE_HEADER,
    NOAH_PROFILE_STORE_COMMIT_SHAPE_DOMAIN,
    NOAH_PROFILE_STORE_COMMIT_PREPARED_MARKER_WRITE,
    NOAH_PROFILE_STORE_COMMIT_PREPARED_MARKER_READBACK,
    NOAH_PROFILE_STORE_COMMIT_MARKER_WRITE,
    NOAH_PROFILE_STORE_COMMIT_MARKER_READBACK,
} noah_profile_store_commit_phase_t;

typedef enum {
    NOAH_PROFILE_STORE_BOOT_IDLE = 0u,
    NOAH_PROFILE_STORE_BOOT_SLOT_A_HEADER,
    NOAH_PROFILE_STORE_BOOT_SLOT_A_PAYLOAD,
    NOAH_PROFILE_STORE_BOOT_SLOT_A_SHAPE_HEADER,
    NOAH_PROFILE_STORE_BOOT_SLOT_A_SHAPE_DOMAIN,
    NOAH_PROFILE_STORE_BOOT_SLOT_B_HEADER,
    NOAH_PROFILE_STORE_BOOT_SLOT_B_PAYLOAD,
    NOAH_PROFILE_STORE_BOOT_SLOT_B_SHAPE_HEADER,
    NOAH_PROFILE_STORE_BOOT_SLOT_B_SHAPE_DOMAIN,
    NOAH_PROFILE_STORE_BOOT_DONE,
} noah_profile_store_boot_phase_t;

typedef bool (*noah_profile_store_read_fn)(void *context, uint16_t address, uint8_t *target, uint16_t length);
typedef bool (*noah_profile_store_write_fn)(void *context, uint16_t address, const uint8_t *source, uint16_t length);

typedef struct {
    noah_profile_store_read_fn  read;
    noah_profile_store_write_fn write;
    void                       *context;
} noah_profile_store_io_t;

// Optional destructive-write guard. A configured store calls begin after all
// candidate checks pass but before it invalidates the target slot. Exactly one
// matching end follows every admitted prepare, including abort and all
// terminal I/O/validation failures. Callbacks must not perform store I/O.
typedef bool (*noah_profile_store_reuse_begin_fn)(void *context, noah_profile_slot_t slot);
typedef bool (*noah_profile_store_reuse_end_fn)(void *context, noah_profile_slot_t slot);

typedef struct {
    noah_profile_store_reuse_begin_fn begin;
    noah_profile_store_reuse_end_fn   end;
    void                             *context;
} noah_profile_store_reuse_guard_t;

typedef struct {
    uint8_t schema_major;
    uint8_t schema_minor;
    // Persisted profiles are compatible only with the authored defaults and
    // semantic action vocabulary that existed when they were encoded.
    uint32_t compiled_default_digest;
    uint32_t action_abi_digest;
} noah_profile_store_compatibility_t;

typedef struct {
    noah_profile_slot_t slot;
    uint8_t             format_version;
    uint8_t             schema_major;
    uint8_t             schema_minor;
    // Derived from the checksummed canonical payload during slot validation;
    // it is not an independent field in the fixed 32-byte header.
    uint8_t  domain_mask;
    uint8_t  flags;
    uint8_t  origin_half;
    uint16_t payload_length;
    uint32_t generation;
    uint32_t payload_crc32;
    uint32_t payload_digest;
    uint32_t compiled_default_digest;
    uint32_t action_abi_digest;
    uint32_t via_generation;
    uint32_t via_digest;
} noah_profile_store_record_t;

typedef struct {
    // Zero keeps legacy format 1 for old callers. Logical transactions select
    // format 2 and must bind a nonzero VIA generation and digest.
    uint8_t format_version;
    uint8_t schema_major;
    uint8_t schema_minor;
    // The marker-last shape pass must derive this exact mask before commit.
    uint8_t  domain_mask;
    uint8_t  flags;
    uint8_t  origin_half;
    uint16_t payload_length;
    uint32_t generation;
    uint32_t payload_crc32;
    uint32_t payload_digest;
    uint32_t compiled_default_digest;
    uint32_t action_abi_digest;
    uint32_t via_generation;
    uint32_t via_digest;
} noah_profile_store_candidate_t;

typedef struct {
    noah_profile_store_io_t            io;
    noah_profile_store_reuse_guard_t   reuse_guard;
    noah_profile_store_compatibility_t compatibility;
    noah_profile_store_record_t        committed;
    union {
        noah_profile_store_candidate_t candidate;
        struct {
            noah_profile_store_record_t boot_slot_a;
            noah_profile_store_record_t boot_current;
        };
    };
    noah_profile_slot_t                candidate_slot;
    uint16_t                           candidate_written;
    uint32_t                           candidate_crc32_state;
    uint32_t                           candidate_digest_state;
    noah_profile_store_commit_phase_t  commit_phase;
    uint16_t                           commit_offset;
    uint32_t                           commit_crc32_state;
    uint32_t                           commit_digest_state;
    uint8_t                            commit_domain_count;
    uint8_t                            commit_domain_index;
    uint8_t                            commit_prior_domain;
    uint8_t                            commit_domain_mask;
    uint8_t                            commit_record_offset;
    bool                               boot_scanned;
    bool                               conflict;
    bool                               reconciliation_required;
    bool                               prepare_active;
    bool                               prepared_durable;
    bool                               auto_commit_prepared;
    bool                               reuse_active;
    uint8_t                            scratch[NOAH_PROFILE_STORE_IO_CHUNK_MAX];
    noah_profile_store_result_t        boot_slot_a_result;
    noah_profile_store_result_t        boot_slot_b_result;
    noah_profile_store_result_t        boot_result;
    noah_profile_store_boot_phase_t    boot_phase;
    uint16_t                           boot_payload_start;
    uint16_t                           boot_offset;
    uint32_t                           boot_crc32_state;
    uint32_t                           boot_digest_state;
    uint8_t                            boot_domain_count;
    uint8_t                            boot_domain_index;
    uint8_t                            boot_prior_domain;
    uint8_t                            boot_expected_domain_mask;
} noah_profile_store_t;

void                        noah_profile_store_init(noah_profile_store_t *store, noah_profile_store_io_t io, noah_profile_store_compatibility_t compatibility);
bool                        noah_profile_store_set_reuse_guard(noah_profile_store_t *store, const noah_profile_store_reuse_guard_t *guard);
noah_profile_store_result_t noah_profile_store_boot_select(noah_profile_store_t *store, noah_profile_store_record_t *selected);
// Production boot discovery. Begin performs no I/O. Each step performs at
// most one bounded read. Payload checksum reads honor byte_budget; fixed
// header/shape reads are at most NOAH_PROFILE_STORE_IO_CHUNK_MAX. The terminal
// result is idempotent.
noah_profile_store_result_t noah_profile_store_boot_select_begin(noah_profile_store_t *store);
noah_profile_store_result_t noah_profile_store_boot_select_step(noah_profile_store_t *store, uint8_t byte_budget, noah_profile_store_record_t *selected);
noah_profile_store_result_t noah_profile_store_validate_slot(noah_profile_store_t *store, noah_profile_slot_t slot, bool require_commit, noah_profile_store_record_t *record);
noah_profile_store_result_t noah_profile_store_prepare_begin(noah_profile_store_t *store, const noah_profile_store_candidate_t *candidate);
noah_profile_store_result_t noah_profile_store_prepare_write(noah_profile_store_t *store, uint16_t offset, const uint8_t *bytes, uint16_t length);
// Validates the staged payload and persists transaction intent without making
// it active authority. A prepared slot is ignored by ordinary boot selection
// and may still be aborted safely.
noah_profile_store_result_t noah_profile_store_prepare_durable_begin(noah_profile_store_t *store);
noah_profile_store_result_t noah_profile_store_prepare_durable_step(noah_profile_store_t *store, uint8_t byte_budget, noah_profile_store_record_t *prepared);
// Replaces the durable prepared marker with the logical decision marker. Once
// this begins, failure is durability-unknown and must be resolved by boot scan.
noah_profile_store_result_t noah_profile_store_prepared_commit_begin(noah_profile_store_t *store);
noah_profile_store_result_t noah_profile_store_prepared_commit_step(noah_profile_store_t *store, uint8_t byte_budget, noah_profile_store_record_t *committed);
// Starts and advances a marker-last commit without unbounded scan work. Begin
// performs no EEPROM I/O. This compatibility API runs both durable prepare and
// decision phases. Each step performs exactly one read or write of at most
// byte_budget bytes; byte_budget must be 1..20. The final OK step returns the
// committed record and releases the destructive-backing reservation.
noah_profile_store_result_t noah_profile_store_prepare_commit_begin(noah_profile_store_t *store);
noah_profile_store_result_t noah_profile_store_prepare_commit_step(noah_profile_store_t *store, uint8_t byte_budget, noah_profile_store_record_t *committed);
// Cold/test convenience wrapper around begin + bounded steps.
noah_profile_store_result_t noah_profile_store_prepare_commit(noah_profile_store_t *store, noah_profile_store_record_t *committed);
noah_profile_store_result_t noah_profile_store_prepare_abort(noah_profile_store_t *store);
bool                        noah_profile_store_next_generation(const noah_profile_store_t *store, uint32_t *generation);
