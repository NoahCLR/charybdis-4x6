// Paint 4, map: the map's value at the LED, a palette colour or the
// parameterless paint 0x80 + k names.
#include "../lighting.h"

#if NOAH_LIGHTING_PAINT_MAP

static uint8_t map_value(const noah_profile_lighting_v1_program_t *program, uint8_t map, uint8_t led) {
    const uint8_t *bytes = &program->bytes[program->maps[map]];

    if (bytes[0] == 0u) return led < NOAH_PROFILE_LIGHTING_V1_LED_COUNT ? bytes[1u + led] : NOAH_PROFILE_LIGHTING_V1_MAP_CLEAR;
    for (uint8_t entry = 0u; entry < bytes[1]; entry++) {
        if (bytes[2u + 2u * entry] == led) return bytes[3u + 2u * entry];
    }
    return NOAH_PROFILE_LIGHTING_V1_MAP_CLEAR;
}

bool noah_lighting_paint_map(const noah_lighting_paint_context_t *context, rgb_t *colour) {
    uint8_t                       value = map_value(context->scene->program, context->params[0], context->led);
    noah_lighting_paint_context_t named = *context;

    if (value < NOAH_PROFILE_LIGHTING_V1_MAP_PAINT) {
        *colour = noah_lighting_palette_rgb(context->scene, value);
        return true;
    }
    named.params = NULL;
    return noah_lighting_paint((uint8_t)(value - NOAH_PROFILE_LIGHTING_V1_MAP_PAINT), &named, colour);
}

#endif
