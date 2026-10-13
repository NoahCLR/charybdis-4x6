// The parity oracle's stand-in for the retired effective RGB frame: the old
// stage renderer read its version-4 view through a frame token. Here every
// frame is current and serves the configuration under test.
#pragma once

#include <stdbool.h>

#include "users/noah/lib/profile/schema/profile_rgb_v1.h"

typedef struct {
    bool valid;
} noah_effective_rgb_frame_t;

typedef enum {
    NOAH_EFFECTIVE_RGB_OK = 0u,
    NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK,
    NOAH_EFFECTIVE_RGB_STALE,
    NOAH_EFFECTIVE_RGB_BUSY,
    NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT,
} noah_effective_rgb_result_t;

// The configuration's version-4 view, supplied by the parity driver.
const noah_profile_rgb_v1_view_t *legacy_rgb_v4_view(void);
