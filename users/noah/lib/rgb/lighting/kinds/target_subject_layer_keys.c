// Target 6, the subject layer's mapped keys: LEDs of keys that are neither
// KC_NO nor KC_TRNS on the subject's layer, read live from the keymap and
// cached per layer until the keymap changes.
#include "../lighting_keymap.h"

#if NOAH_LIGHTING_TARGET_SUBJECT_LAYER_KEYS

#    include "keymap_introspection.h" // QMK

#if !NOAH_LIGHTING_KEYMAP_CACHE
static noah_lighting_leds_t mapped[LAYER_COUNT];
static uint32_t             cached_generation;
static uint32_t             cached_layers; // one bit per layer whose mask is current

static noah_lighting_leds_t mapped_leds(uint8_t layer) {
    noah_lighting_leds_t leds = 0u;

    for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
            uint16_t keycode = keycode_at_keymap_location(layer, row, col);
            uint8_t  found[NOAH_LIGHTING_LED_COUNT];
            uint8_t  count;

            if (keycode == KC_NO || keycode == KC_TRNS) continue;
            count = rgb_matrix_map_row_column_to_led(row, col, found);
            for (uint8_t index = 0u; index < count; index++) {
                if (found[index] < NOAH_LIGHTING_LED_COUNT) leds |= NOAH_LIGHTING_LED_BIT(found[index]);
            }
        }
    }
    return leds;
}

noah_lighting_leds_t noah_lighting_target_subject_layer_keys(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    uint8_t layer = subject->thing;

    (void)params;
    if (subject->thing_kind != NOAH_LIGHTING_THING_LAYER || layer >= LAYER_COUNT) return 0u;
    if (cached_generation != scene->keymap_generation) {
        cached_generation = scene->keymap_generation;
        cached_layers     = 0u;
    }
    if ((cached_layers & (UINT32_C(1) << layer)) == 0u) {
        mapped[layer] = mapped_leds(layer);
        cached_layers |= UINT32_C(1) << layer;
    }
    return mapped[layer];
}

#else
noah_lighting_leds_t noah_lighting_target_subject_layer_keys(const noah_lighting_scene_t *scene, const uint8_t *params, const noah_lighting_subject_t *subject) {
    (void)params;
    (void)scene;
    if (subject->thing_kind != NOAH_LIGHTING_THING_LAYER || subject->thing >= LAYER_COUNT) return 0u;
    uint8_t keys[KEY_ORIGIN_BITMAP_SIZE] = {0};
    for (uint8_t key = 0u; key < NOAH_LIGHTING_MATRIX_KEYS; key++) {
        if (noah_lighting_keymap_matches(subject->thing, key, NOAH_LIGHTING_QUERY_MAPPED, 255u)) key_origin_bitmap_add_keypos(keys, (keypos_t){.row = key / MATRIX_COLS, .col = key % MATRIX_COLS});
    }
    return noah_lighting_key_leds(keys);
}
#endif

#endif
