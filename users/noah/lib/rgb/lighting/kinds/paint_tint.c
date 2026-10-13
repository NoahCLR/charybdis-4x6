// Paint 6, tint: below mixed toward a palette colour by amount / 255.
#include "../lighting.h"

#if NOAH_LIGHTING_PAINT_TINT

static uint8_t mix(uint8_t below, uint8_t toward, uint8_t amount) {
    return (uint8_t)(((uint32_t)below * (255u - amount) + (uint32_t)toward * amount + 127u) / 255u);
}

bool noah_lighting_paint_tint(const noah_lighting_paint_context_t *context, rgb_t *colour) {
    rgb_t   below  = noah_lighting_below(context);
    rgb_t   toward = noah_lighting_palette_rgb(context->scene, context->params[0]);
    uint8_t amount = context->params[1];

    *colour = (rgb_t){.r = mix(below.r, toward.r, amount), .g = mix(below.g, toward.g, amount), .b = mix(below.b, toward.b, amount)};
    return true;
}

#endif
