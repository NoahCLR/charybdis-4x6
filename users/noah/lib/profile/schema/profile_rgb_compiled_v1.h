#pragma once
#include "profile_lighting_v1.h"
#include "profile_rgb_v1.h"
// The firmware's own lighting: the authored rgb_config.c tables, encoded as
// RGB domain version 4 and converted to version 5 (Standard lighting) by the
// conversion a client also performs. prepare() builds both caches once; the
// compiled profile's open and the lighting program's load call it, at boot
// depth. The views and the compiled RGB domain stream only read the caches,
// so a reader deep in a transfer never runs the conversion.
bool noah_profile_rgb_compiled_v1_prepare(void);
#if defined(RGB_MATRIX_ENABLE)
const noah_profile_lighting_v1_view_t *noah_profile_rgb_compiled_v1_view(void);
const noah_profile_rgb_v1_view_t      *noah_profile_rgb_compiled_v1_v4_view(void);
#else
static inline const noah_profile_lighting_v1_view_t *noah_profile_rgb_compiled_v1_view(void) {
    return NULL;
}
#endif
// What this build supports: its registry rows, layers, depth and brightness.
noah_profile_lighting_v1_limits_t noah_profile_rgb_compiled_v1_limits(void);
