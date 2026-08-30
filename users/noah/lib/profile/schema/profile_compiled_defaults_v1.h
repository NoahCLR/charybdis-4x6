// ────────────────────────────────────────────────────────────────────────────
// Compiled Authored Defaults — Profile Wire v1.0
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "key_behavior_domain_v1.h"
#include "profile_blob_v1.h"
#include "profile_reader.h"
#include "profile_validator_v1.h"

enum {
    NOAH_PROFILE_COMPILED_V1_DOMAIN_MASK_RGB           = 1u << 0,
    NOAH_PROFILE_COMPILED_V1_DOMAIN_MASK_KEY_BEHAVIORS = 1u << 1,
    NOAH_PROFILE_COMPILED_V1_DOMAIN_MASK_ALL           = NOAH_PROFILE_COMPILED_V1_DOMAIN_MASK_RGB | NOAH_PROFILE_COMPILED_V1_DOMAIN_MASK_KEY_BEHAVIORS,
    // The virtual reader may replay canonical records from byte zero through
    // the requested slice. This is a cold identity/export path, never the
    // effective-profile lookup path used by RGB frames or key events.
    NOAH_PROFILE_COMPILED_V1_READER_REPLAY_MAX          = NOAH_PROFILE_BLOB_V1_MAX_SIZE,
    // ABI vocabulary canonicalization scans at most the 64 compiled behavior
    // targets once to count and once per possible stable custom target.
    NOAH_PROFILE_COMPILED_V1_ACTION_ABI_ROW_VISITS_MAX  = NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS * (NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS + 1u),
};

typedef enum {
    NOAH_PROFILE_COMPILED_V1_OK = 0u,
    NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT,
    NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED,
    NOAH_PROFILE_COMPILED_V1_INVALID_ACTION,
    NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR,
    NOAH_PROFILE_COMPILED_V1_INVALID_RGB,
    NOAH_PROFILE_COMPILED_V1_WRITE_ERROR,
} noah_profile_compiled_v1_result_t;

typedef enum {
    NOAH_PROFILE_COMPILED_V1_SURFACE_NONE = 0u,
    NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI,
    NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR,
    NOAH_PROFILE_COMPILED_V1_SURFACE_RGB,
} noah_profile_compiled_v1_surface_t;

typedef struct {
    noah_profile_compiled_v1_result_t  code;
    noah_profile_compiled_v1_surface_t surface;
    uint8_t                            row;
    uint8_t                            step;
} noah_profile_compiled_v1_error_t;

typedef struct {
    uint32_t crc32;
    uint32_t digest;
    uint32_t action_abi_digest;
    uint16_t byte_length;
    uint16_t action_abi_row_visits;
    uint8_t domain_mask;
} noah_profile_compiled_v1_metadata_t;

// Payload-independent handle. The canonical bytes remain a virtual view over
// keymap-owned const authored data; no heap or profile-sized persistent buffer
// is retained.
typedef struct {
    noah_profile_compiled_v1_metadata_t metadata;
} noah_profile_compiled_v1_t;

typedef bool (*noah_profile_compiled_v1_write_fn)(void *context, const uint8_t *bytes, size_t length);

// Validates/materializes authored metadata and streams CRC32/FNV-1a over the
// exact canonical blob. This performs no writes and retains only metadata.
noah_profile_compiled_v1_result_t noah_profile_compiled_v1_open(noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_error_t *error);

// Replays the canonical blob in small records. Useful for bounded persistence
// and host fixture generation without a profile-sized firmware buffer.
noah_profile_compiled_v1_result_t noah_profile_compiled_v1_write(const noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_write_fn write, void *context, noah_profile_compiled_v1_error_t *error);

// Cold-path reader for validation, identity, export, and reset persistence.
// An arbitrary read regenerates preceding records and stops after the record
// containing the requested end. Its declared worst-case replay is the frozen
// 4,064-byte blob ceiling above. The reader borrows profile for its lifetime.
// Runtime providers must read compiled defaults directly from authored tables,
// not call this virtual reader from key-event or RGB-frame hot paths.
noah_profile_reader_t noah_profile_compiled_v1_reader(const noah_profile_compiled_v1_t *profile);

// Builds the exact validator/runtime ceilings represented by this firmware
// artifact. Callers may tighten required_domain_mask for a particular
// operation, but must not widen any returned capability.
bool noah_profile_compiled_v1_compatibility(const noah_profile_compiled_v1_t *profile, noah_profile_validator_v1_compatibility_t *compatibility);

// Shared native-to-semantic action translation used by compiled defaults and
// later providers. KC_NO maps to the explicit NONE action; populated behavior
// branches and targets reject it separately.
noah_profile_compiled_v1_result_t noah_profile_compiled_v1_action(uint16_t native_action, noah_profile_action_v1_t *action);
