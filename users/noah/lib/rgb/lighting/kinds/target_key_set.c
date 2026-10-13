// Target 3, key set: the LEDs of a saved key set.
#include "../lighting.h"

#if NOAH_LIGHTING_TARGET_KEY_SET

noah_lighting_leds_t noah_lighting_target_key_set(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    const uint8_t       *bitmap = &scene->program->bytes[scene->program->key_sets + (uint16_t)params[0] * NOAH_PROFILE_LIGHTING_V1_KEY_SET_SIZE];
    noah_lighting_leds_t leds   = 0u;

    (void)subject;
    for (uint8_t index = 0u; index < NOAH_PROFILE_LIGHTING_V1_KEY_SET_SIZE; index++) {
        leds |= (noah_lighting_leds_t)bitmap[index] << (8u * index);
    }
    return leds & NOAH_LIGHTING_ALL_LEDS;
}

#endif
