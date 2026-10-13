// Target 7, the subject's place: the target its thing record names, or none
// for a kind without a table.
#include "../lighting.h"

#if NOAH_LIGHTING_TARGET_SUBJECT_PLACE

noah_lighting_leds_t noah_lighting_target_subject_place(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    uint8_t place;

    (void)params;
    if (!noah_profile_lighting_v1_program_thing_field(scene->program, subject->thing_kind, subject->thing, 0u, &place) || place == NOAH_LIGHTING_TARGET_ID_SUBJECT_PLACE) return 0u;
    return noah_lighting_target(place, scene, NULL, subject);
}

#endif
