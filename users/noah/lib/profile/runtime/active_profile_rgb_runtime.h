// ────────────────────────────────────────────────────────────────────────────
// Active-Profile RGB Runtime View
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "active_profile_provider.h"

enum {
    // Caller-owned derived-state policies on a 32-bit target. These are not
    // RP2040 capacity statements. Instances are allocated only by the
    // explicit engineering profile-owner artifact, not ordinary firmware.
    NOAH_ACTIVE_PROFILE_RGB_RUNTIME_STATE_BUDGET_32BIT = 192u,
    NOAH_ACTIVE_PROFILE_RGB_FRAME_BUDGET_32BIT         = 80u,
};

typedef enum {
    NOAH_ACTIVE_PROFILE_RGB_OK = 0u,
    NOAH_ACTIVE_PROFILE_RGB_COMPILED_FALLBACK,
    NOAH_ACTIVE_PROFILE_RGB_STALE,
    NOAH_ACTIVE_PROFILE_RGB_BUSY,
    NOAH_ACTIVE_PROFILE_RGB_INVALID_ARGUMENT,
} noah_active_profile_rgb_result_t;

// A frame carries a copied reader-backed view, never a pointer into a runtime
// bank. Access must first pass frame_status(), which refuses an epoch that was
// superseded after the frame began.
typedef struct {
    noah_active_profile_identity_t identity;
    noah_profile_rgb_v1_view_t     view;
    uint32_t                       publication_count;
    uint32_t                       epoch;
    bool                           live;
    bool                           valid;
} noah_active_profile_rgb_frame_t;

typedef struct {
    noah_runtime_publication_generation_t publication_sequence;
    noah_active_profile_rgb_frame_t       banks[2];
    uint32_t                              next_epoch;
    uint8_t                               active_index;
    bool                                  initialized;
} noah_active_profile_rgb_runtime_t;

void noah_active_profile_rgb_runtime_init(noah_active_profile_rgb_runtime_t *runtime);
bool noah_active_profile_rgb_runtime_install(noah_active_profile_rgb_runtime_t *runtime);
void noah_active_profile_rgb_runtime_uninstall(noah_active_profile_rgb_runtime_t *runtime);

// Initial compiled invalidation warms the immutable RGB cache on the cold
// owner boot path. Later publications copy the validated stored or cached
// factory view and swap a double bank, without constructing a color cache.
void noah_active_profile_rgb_runtime_invalidate(void *context, uint32_t publication_count, noah_active_profile_identity_t previous, noah_active_profile_identity_t active, const noah_active_profile_snapshot_t *callback_view);

noah_active_profile_rgb_result_t noah_active_profile_rgb_runtime_capture_frame(const noah_active_profile_rgb_runtime_t *runtime, noah_active_profile_rgb_frame_t *frame);
noah_active_profile_rgb_result_t noah_active_profile_rgb_runtime_frame_status(const noah_active_profile_rgb_runtime_t *runtime, const noah_active_profile_rgb_frame_t *frame);

// Installed-runtime wrappers. With no owner the active-profile RGB adapter uses
// the same immutable compiled-domain cache.
noah_active_profile_rgb_result_t noah_active_profile_rgb_capture_frame(noah_active_profile_rgb_frame_t *frame);
noah_active_profile_rgb_result_t noah_active_profile_rgb_frame_status(const noah_active_profile_rgb_frame_t *frame);

// Scan-owner invalidation stamp for derived matrix coverage maps.
uint32_t noah_active_profile_rgb_publication_epoch(void);

#ifdef NOAH_ACTIVE_PROFILE_RGB_RUNTIME_TEST_BACKEND
// Whole-frame copies out of a bank since the last reset, across runtimes.
void     noah_active_profile_rgb_runtime_test_reset_frame_copies(void);
uint32_t noah_active_profile_rgb_runtime_test_frame_copies(void);
#endif
