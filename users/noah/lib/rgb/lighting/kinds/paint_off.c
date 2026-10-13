// Paint 1, off: black.
#include "../lighting.h"

#if NOAH_LIGHTING_PAINT_OFF

bool noah_lighting_paint_off(const noah_lighting_paint_context_t *context, rgb_t *colour) {
    (void)context;
    *colour = (rgb_t){0};
    return true;
}

#endif
