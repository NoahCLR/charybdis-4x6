// Paint 5, dim: below scaled by level / 255.
#include "../lighting.h"

#if NOAH_LIGHTING_PAINT_DIM

static uint8_t scale(uint8_t channel, uint8_t level) {
    return (uint8_t)(((uint16_t)channel * level + 127u) / 255u);
}

bool noah_lighting_paint_dim(const noah_lighting_paint_context_t *context, rgb_t *colour) {
    rgb_t below = noah_lighting_below(context);
    uint8_t level = context->params[0];

    *colour = (rgb_t){.r = scale(below.r, level), .g = scale(below.g, level), .b = scale(below.b, level)};
    return true;
}

#endif
