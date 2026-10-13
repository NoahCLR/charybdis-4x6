// Target 4, the subject's keys: their LEDs, or the whole board when the
// subject has none.
#include "../lighting.h"

#if NOAH_LIGHTING_TARGET_SUBJECT_KEYS

noah_lighting_leds_t noah_lighting_target_subject_keys(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    (void)scene;
    (void)params;
    return subject->has_keys ? noah_lighting_key_leds(subject->keys) : NOAH_LIGHTING_ALL_LEDS;
}

#endif
