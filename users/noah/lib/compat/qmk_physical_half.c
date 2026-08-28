// ──────────────────────────────────────────────────────────────────────────
// QMK Physical-Half Identity Boundary
// ──────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "qmk_physical_half.h"

#if defined(NOAH_PHYSICAL_HALF_LEFT) && defined(NOAH_PHYSICAL_HALF_RIGHT)
#    error "a firmware artifact must provision exactly one physical half"
#endif

bool noah_qmk_physical_half_origin(uint8_t *origin) {
    if (!origin) {
        return false;
    }

#if defined(NOAH_PHYSICAL_HALF_LEFT)
    *origin = NOAH_PHYSICAL_HALF_ORIGIN_LEFT;
    return true;
#elif defined(NOAH_PHYSICAL_HALF_RIGHT)
    *origin = NOAH_PHYSICAL_HALF_ORIGIN_RIGHT;
    return true;
#else
    *origin = 0u;
    return false;
#endif
}

// QMK's weak no-hand-pin/no-EE_HANDS fallback derives this value from current
// master role. Side-specific artifacts override it with the flash-provisioned
// physical identity so role swaps cannot change handedness or profile origin.
#if defined(NOAH_PHYSICAL_HALF_LEFT)
bool is_keyboard_left_impl(void) {
    return true;
}
#elif defined(NOAH_PHYSICAL_HALF_RIGHT)
bool is_keyboard_left_impl(void) {
    return false;
}
#endif
