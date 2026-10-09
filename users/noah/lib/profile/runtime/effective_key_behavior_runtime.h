// ────────────────────────────────────────────────────────────────────────────
// Effective Live Key-Behavior Runtime View
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "effective_profile_provider.h"
#include "../../key/behavior/key_behavior.h"

enum {
    // Caller-owned derived state policy on a 32-bit target. This is not an
    // RP2040 capacity statement. Most of it is the two banks' row indexes,
    // 2 × 128 offsets of two bytes.
    NOAH_EFFECTIVE_KEY_BEHAVIOR_RUNTIME_STATE_BUDGET_32BIT = 768u,
};

typedef enum {
    NOAH_EFFECTIVE_KEY_BEHAVIOR_OK = 0u,
    NOAH_EFFECTIVE_KEY_BEHAVIOR_COMPILED_FALLBACK,
    NOAH_EFFECTIVE_KEY_BEHAVIOR_NOT_FOUND,
    NOAH_EFFECTIVE_KEY_BEHAVIOR_STALE,
    NOAH_EFFECTIVE_KEY_BEHAVIOR_BUSY,
    NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ARGUMENT,
    NOAH_EFFECTIVE_KEY_BEHAVIOR_READ_ERROR,
    NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ACTION,
} noah_effective_key_behavior_result_t;

typedef struct {
    noah_effective_profile_identity_t identity;
    noah_key_behavior_domain_v1_t     domain;
    uint32_t                          publication_count;
    uint32_t                          epoch;
    bool                              live;
    bool                              valid;
    // The bank's row index is usable; otherwise lookups scan the rows.
    bool                              indexed;
    uint8_t                           bank;
} noah_effective_key_behavior_snapshot_t;

typedef struct {
    noah_runtime_publication_generation_t  publication_sequence;
    noah_effective_key_behavior_snapshot_t banks[2];
    // Each bank's row offsets, built at publication. They stay here rather
    // than in the snapshot, so a lookup's stack copy carries none of them.
    uint16_t                               row_offsets[2][NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS];
    uint32_t                               next_epoch;
    uint8_t                                active_index;
    bool                                   initialized;
} noah_effective_key_behavior_runtime_t;

// A lookup token contains no payload pointer. The epoch prevents a later step
// lookup from accidentally resolving against a different published profile.
typedef struct {
    uint32_t            epoch;
    uint8_t             row_index;
    uint8_t             flags;
    uint8_t             step_count;
    uint8_t             authored_tap_depth;
    uint16_t            tap_hold_term;
    uint16_t            longer_hold_term;
    uint16_t            multi_tap_term;
    uint32_t            allowed_layers; // participation: the layers the row may act on
    key_behavior_step_t single;
} noah_effective_key_behavior_row_t;

void noah_effective_key_behavior_runtime_init(noah_effective_key_behavior_runtime_t *runtime);
bool noah_effective_key_behavior_runtime_install(noah_effective_key_behavior_runtime_t *runtime);
void noah_effective_key_behavior_runtime_uninstall(noah_effective_key_behavior_runtime_t *runtime);

// Provider invalidator: copies the already-validated reader-backed view, walks
// the row lengths once into the incoming bank's index (one two-byte read per
// row, at most 128), and swaps the double bank.
void noah_effective_key_behavior_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view);

noah_effective_key_behavior_result_t noah_effective_key_behavior_runtime_lookup(const noah_effective_key_behavior_runtime_t *runtime, uint16_t keycode, noah_effective_key_behavior_row_t *row);
noah_effective_key_behavior_result_t noah_effective_key_behavior_runtime_step(const noah_effective_key_behavior_runtime_t *runtime, uint32_t epoch, uint8_t row_index, uint8_t tap_count, key_behavior_step_t *step);
noah_effective_key_behavior_result_t noah_effective_key_behavior_runtime_status(const noah_effective_key_behavior_runtime_t *runtime, noah_effective_key_behavior_snapshot_t *snapshot);

// Installed-runtime wrappers used by key_behavior_lookup.c. With no installed
// owner they explicitly select the compiled authored tables.
noah_effective_key_behavior_result_t noah_effective_key_behavior_lookup(uint16_t keycode, noah_effective_key_behavior_row_t *row);
noah_effective_key_behavior_result_t noah_effective_key_behavior_step(uint32_t epoch, uint8_t row_index, uint8_t tap_count, key_behavior_step_t *step);
