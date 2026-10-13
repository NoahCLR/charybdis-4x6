// Target 5, the subject's halves: the halves it concerns, or the whole board
// when it concerns none.
#include "../lighting.h"

#if NOAH_LIGHTING_TARGET_SUBJECT_HALVES

noah_lighting_leds_t noah_lighting_target_subject_halves(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    noah_lighting_leds_t leds = 0u;

    (void)scene;
    (void)params;
    if (subject->sides & SPLIT_SIDE_MASK_LEFT) leds |= NOAH_LIGHTING_LEFT_LEDS;
    if (subject->sides & SPLIT_SIDE_MASK_RIGHT) leds |= NOAH_LIGHTING_RIGHT_LEDS;
    return leds ? leds : NOAH_LIGHTING_ALL_LEDS;
}

#endif
