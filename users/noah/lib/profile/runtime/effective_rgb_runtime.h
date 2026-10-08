// ────────────────────────────────────────────────────────────────────────────
// Effective Live RGB Runtime View
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "effective_profile_provider.h"

enum {
    // Caller-owned derived-state policies on a 32-bit target. These are not
    // RP2040 capacity statements. Instances are allocated only by the
    // explicit engineering profile-owner artifact, not ordinary firmware.
    NOAH_EFFECTIVE_RGB_RUNTIME_STATE_BUDGET_32BIT = 192u,
    NOAH_EFFECTIVE_RGB_FRAME_BUDGET_32BIT         = 80u,
};

typedef enum {
    NOAH_EFFECTIVE_RGB_OK = 0u,
    NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK,
    NOAH_EFFECTIVE_RGB_STALE,
    NOAH_EFFECTIVE_RGB_BUSY,
    NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT,
} noah_effective_rgb_result_t;

// A frame carries a copied reader-backed view, never a pointer into a runtime
// bank. Access must first pass frame_status(), which refuses an epoch that was
// superseded after the frame began.
typedef struct {
    noah_effective_profile_identity_t identity;
    noah_profile_rgb_v1_view_t        view;
    uint32_t                          publication_count;
    uint32_t                          epoch;
    bool                              live;
    bool                              valid;
} noah_effective_rgb_frame_t;

typedef struct {
    noah_runtime_publication_generation_t publication_sequence;
    noah_effective_rgb_frame_t            banks[2];
    uint32_t                              next_epoch;
    uint8_t                               active_index;
    bool                                  initialized;
} noah_effective_rgb_runtime_t;

void noah_effective_rgb_runtime_init(noah_effective_rgb_runtime_t *runtime);
bool noah_effective_rgb_runtime_install(noah_effective_rgb_runtime_t *runtime);
void noah_effective_rgb_runtime_uninstall(noah_effective_rgb_runtime_t *runtime);

// Initial compiled invalidation warms the immutable RGB cache on the cold
// owner boot path. Later publications copy the validated stored or cached
// factory view and swap a double bank, without constructing a color cache.
void noah_effective_rgb_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view);

noah_effective_rgb_result_t noah_effective_rgb_runtime_capture_frame(const noah_effective_rgb_runtime_t *runtime, noah_effective_rgb_frame_t *frame);
noah_effective_rgb_result_t noah_effective_rgb_runtime_frame_status(const noah_effective_rgb_runtime_t *runtime, const noah_effective_rgb_frame_t *frame);

// Installed-runtime wrappers. With no owner the effective RGB adapter uses
// the same immutable compiled-domain cache.
noah_effective_rgb_result_t noah_effective_rgb_capture_frame(noah_effective_rgb_frame_t *frame);
noah_effective_rgb_result_t noah_effective_rgb_frame_status(const noah_effective_rgb_frame_t *frame);
