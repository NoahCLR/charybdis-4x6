#pragma once
#include "profile_rgb_v1.h"
#if defined(RGB_MATRIX_ENABLE)
// Cold initialization produces an immutable memory-backed RGB view. Subsequent
// calls borrow the immutable view; frame access never invokes an authored writer.
const noah_profile_rgb_v1_view_t *noah_profile_rgb_compiled_v1_view(void);
#else
static inline const noah_profile_rgb_v1_view_t *noah_profile_rgb_compiled_v1_view(void) {
    return NULL;
}
#endif
