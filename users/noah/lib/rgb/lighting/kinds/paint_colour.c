// Paint 2, colour: a palette colour.
#include "../lighting.h"

#if NOAH_LIGHTING_PAINT_COLOUR

bool noah_lighting_paint_colour(const noah_lighting_paint_context_t *context, rgb_t *colour) {
    *colour = noah_lighting_palette_rgb(context->scene, context->params[0]);
    return true;
}

#endif
