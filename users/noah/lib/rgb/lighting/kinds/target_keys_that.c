#include "../lighting_keymap.h"
#if NOAH_LIGHTING_TARGET_KEYS_THAT
noah_lighting_leds_t noah_lighting_target_keys_that(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    uint8_t keys[KEY_ORIGIN_BITMAP_SIZE] = {0};
    uint8_t layer = noah_lighting_shown_layer(scene, subject);
    for (uint8_t key = 0u; key < NOAH_LIGHTING_MATRIX_KEYS; key++) {
        if (noah_lighting_keymap_matches(layer, key, params[0], params[1])) key_origin_bitmap_add_keypos(keys, (keypos_t){.row = key / MATRIX_COLS, .col = key % MATRIX_COLS});
    }
    return noah_lighting_key_leds(keys);
}
#endif
