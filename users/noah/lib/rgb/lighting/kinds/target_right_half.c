// Target 2, right half: LEDs from the split on.
#include "../lighting.h"

#if NOAH_LIGHTING_TARGET_RIGHT_HALF

noah_lighting_leds_t noah_lighting_target_right_half(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    (void)scene;
    (void)params;
    (void)subject;
    return NOAH_LIGHTING_RIGHT_LEDS;
}

#endif
