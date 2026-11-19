#pragma once
#include "profile_rgb_v1.h"

// The compiled lighting keeps D-F14's ceilings, 16 saved LED sets and 32 group
// rows in all, though a profile holds 128 and 640 (D-F63): its writer numbers
// each row's LED set by comparing it with every row, which a profile from a
// client never goes through.
enum {
    NOAH_PROFILE_RGB_COMPILED_V1_MAX_SAVED_GROUPS = 16u,
    NOAH_PROFILE_RGB_COMPILED_V1_MAX_GROUP_ROWS   = 32u,
};
#if defined(RGB_MATRIX_ENABLE)
// Cold initialization produces an immutable memory-backed RGB view. Subsequent
// calls borrow the immutable view; frame access never invokes an authored writer.
const noah_profile_rgb_v1_view_t *noah_profile_rgb_compiled_v1_view(void);
#else
static inline const noah_profile_rgb_v1_view_t *noah_profile_rgb_compiled_v1_view(void) {
    return NULL;
}
#endif
