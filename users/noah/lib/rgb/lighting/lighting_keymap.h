#pragma once
#include "lighting.h"

// These queries describe native usages/actions, never host-layout characters.
enum {
    NOAH_LIGHTING_QUERY_MAPPED, NOAH_LIGHTING_QUERY_TRANSPARENT, NOAH_LIGHTING_QUERY_NONE,
    NOAH_LIGHTING_QUERY_MODIFIER, NOAH_LIGHTING_QUERY_LETTER, NOAH_LIGHTING_QUERY_NUMBER,
    NOAH_LIGHTING_QUERY_SYMBOL, NOAH_LIGHTING_QUERY_NAVIGATION, NOAH_LIGHTING_QUERY_MEDIA,
    NOAH_LIGHTING_QUERY_MOUSE, NOAH_LIGHTING_QUERY_LAYER, NOAH_LIGHTING_QUERY_MODE,
    NOAH_LIGHTING_QUERY_MACRO, NOAH_LIGHTING_QUERY_CUSTOM, NOAH_LIGHTING_QUERY_BEHAVIOUR,
    NOAH_LIGHTING_QUERY_COMBO,
};

#if NOAH_LIGHTING_KEYMAP_CACHE
// Main context only, before resolving a frame. In-place rebuild cannot be
// read by a renderer until the before/after publication generation agrees.
bool noah_lighting_keymap_refresh(uint32_t generation);
void noah_lighting_keymap_invalidate(void);
bool noah_lighting_keymap_matches(uint8_t layer, uint8_t key, uint8_t query, uint8_t selector);
uint8_t noah_lighting_keymap_led_key(uint8_t led);
bool noah_lighting_keymap_identity(uint8_t layer, uint8_t key, uint8_t *kind, uint8_t *id);
uint8_t noah_lighting_shown_layer(const noah_lighting_scene_t *scene, const noah_lighting_subject_t *subject);
#ifdef NOAH_LIGHTING_KEYMAP_TEST_BACKEND
uint32_t noah_lighting_keymap_test_comparisons(void);
void noah_lighting_keymap_test_reset(void);
#endif
#endif
