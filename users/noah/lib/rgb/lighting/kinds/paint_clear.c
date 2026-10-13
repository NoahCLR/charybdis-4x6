// Paint 0, clear: says nothing; what is below shows.
#include "../lighting.h"

#if NOAH_LIGHTING_PAINT_CLEAR

bool noah_lighting_paint_clear(const noah_lighting_paint_context_t *context, rgb_t *colour) {
    (void)context;
    (void)colour;
    return false;
}

#endif
