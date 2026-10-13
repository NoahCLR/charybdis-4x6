// Target 0, whole board: the whole board.
#include "../lighting.h"

#if NOAH_LIGHTING_TARGET_WHOLE_BOARD

noah_lighting_leds_t noah_lighting_target_whole_board(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    (void)scene;
    (void)params;
    (void)subject;
    return NOAH_LIGHTING_ALL_LEDS;
}

#endif
