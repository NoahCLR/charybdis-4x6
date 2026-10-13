// Paint 3, identity: the subject's identity colour from its thing record;
// clear when it has none.
#include "../lighting.h"

#if NOAH_LIGHTING_PAINT_IDENTITY

bool noah_lighting_paint_identity(const noah_lighting_paint_context_t *context, rgb_t *colour) {
    const noah_lighting_subject_t *subject = context->subject;
    uint8_t                        reference;

    if (!noah_profile_lighting_v1_program_thing_field(context->scene->program, subject->thing_kind, subject->thing, (uint8_t)(1u + subject->slot), &reference) || reference == NOAH_PROFILE_LIGHTING_V1_NONE) return false;
    *colour = noah_lighting_palette_rgb(context->scene, reference);
    return true;
}

#endif
