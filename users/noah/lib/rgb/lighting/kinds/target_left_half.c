// Target 1, left half: LEDs before the split.
#include "../lighting.h"

#if NOAH_LIGHTING_TARGET_LEFT_HALF

noah_lighting_leds_t noah_lighting_target_left_half(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    (void)scene;
    (void)params;
    (void)subject;
    return NOAH_LIGHTING_LEFT_LEDS;
}

#endif
