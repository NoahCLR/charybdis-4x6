// Paint 8: identity of the resolved hold/long-hold layer or pointing mode.
// Unsupported outcomes and other slots say clear; black remains real black.
#include "../lighting.h"

#if NOAH_LIGHTING_PAINT_OUTCOME
bool noah_lighting_paint_outcome(const noah_lighting_paint_context_t *context, rgb_t *colour) {
    const noah_lighting_subject_t *subject = context->subject;
    uint8_t                        kind, thing, reference;
    if (subject->thing_kind != NOAH_LIGHTING_THING_BEHAVIOUR || subject->slot > NOAH_LIGHTING_BEHAVIOUR_SLOT_LONG_HOLD) return false;
    if (subject->outcome >= 1u && subject->outcome <= 16u) {
        kind  = NOAH_LIGHTING_THING_LAYER;
        thing = subject->outcome - 1u;
    } else if (subject->outcome >= 33u && subject->outcome <= 64u) {
        kind  = NOAH_LIGHTING_THING_POINTING_MODE;
        thing = subject->outcome - 33u;
    } else
        return false;
    if (!noah_profile_lighting_v1_program_thing_field(context->scene->program, kind, thing, 1u, &reference) || reference == NOAH_PROFILE_LIGHTING_V1_NONE) return false;
    *colour = noah_lighting_palette_rgb(context->scene, reference);
    return true;
}
#endif
