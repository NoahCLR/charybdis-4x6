#include "../lighting_keymap.h"
#if NOAH_LIGHTING_PAINT_WHAT_IT_DOES
bool noah_lighting_paint_what_it_does(const noah_lighting_paint_context_t *context, rgb_t *colour) {
    uint8_t layer = noah_lighting_shown_layer(context->scene, context->subject);
    uint8_t key = noah_lighting_keymap_led_key(context->led);
    uint8_t kind, id, reference;
    if (!noah_lighting_keymap_identity(layer, key, &kind, &id) || !noah_profile_lighting_v1_program_thing_field(context->scene->program, kind, id, 1u, &reference) || reference == NOAH_PROFILE_LIGHTING_V1_NONE) return false;
    *colour = noah_lighting_palette_rgb(context->scene, reference);
    return true;
}
#endif
