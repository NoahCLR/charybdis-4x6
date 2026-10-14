// Exact per-layer query basis. Three bytes per matrix position: native u16
// and the authored behaviour row+1 (zero means absent). All storage reads
// occur before the render walk. No query-count-dependent allocation.
#include "lighting_keymap.h"
#if NOAH_LIGHTING_KEYMAP_CACHE
#    include <string.h>
#    include "keymap_introspection.h"
#    include "noah_keymap_ids.h"
#    include "../../key/behavior/key_behavior_lookup.h"
#    include "../../profile/runtime/effective_combo_runtime.h"
#    include "../../profile/runtime/effective_key_behavior_runtime.h"

static uint16_t native[LAYER_COUNT][NOAH_LIGHTING_MATRIX_KEYS];
static uint8_t behaviour[LAYER_COUNT][NOAH_LIGHTING_MATRIX_KEYS];
static uint8_t combo_inputs[LAYER_COUNT][KEY_ORIGIN_BITMAP_SIZE];
static bool combo_matches(uint16_t code, uint8_t layer, uint8_t selector);
static uint32_t cached_generation;
static bool ready;
#    ifdef NOAH_LIGHTING_KEYMAP_TEST_BACKEND
static uint32_t comparisons;
uint32_t noah_lighting_keymap_test_comparisons(void) { return comparisons; }
void noah_lighting_keymap_test_reset(void) { comparisons = 0u; }
#    endif
static uint8_t led_key[NOAH_LIGHTING_LED_COUNT];

uint8_t noah_lighting_keymap_led_key(uint8_t led) { return ready && led < NOAH_LIGHTING_LED_COUNT ? led_key[led] : 255u; }

void noah_lighting_keymap_invalidate(void) { ready = false; }

static bool capture_behaviour(uint16_t code, uint8_t *identity) {
    noah_effective_key_behavior_row_t row;
    noah_effective_key_behavior_result_t result = noah_effective_key_behavior_lookup(code, &row);
    if (result == NOAH_EFFECTIVE_KEY_BEHAVIOR_OK) *identity = row.row_index + 1u;
    else if (result == NOAH_EFFECTIVE_KEY_BEHAVIOR_NOT_FOUND) *identity = 0u;
    else if (result == NOAH_EFFECTIVE_KEY_BEHAVIOR_COMPILED_FALLBACK) *identity = key_behavior_lookup(code).lighting_identity;
    else return false;
    return true;
}

bool __attribute__((noinline)) noah_lighting_keymap_refresh(uint32_t generation) {
    if (ready && cached_generation == generation) return true;
    ready = false;
    memset(led_key, 255, sizeof(led_key));
    for (uint8_t key = 0u; key < NOAH_LIGHTING_MATRIX_KEYS; key++) {
        uint8_t found[NOAH_LIGHTING_LED_COUNT];
        uint8_t count = rgb_matrix_map_row_column_to_led(key / MATRIX_COLS, key % MATRIX_COLS, found);
        for (uint8_t i = 0u; i < count; i++) if (found[i] < NOAH_LIGHTING_LED_COUNT && led_key[found[i]] == 255u) led_key[found[i]] = key;
    }
    memset(combo_inputs, 0, sizeof(combo_inputs));
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        for (uint8_t key = 0u; key < NOAH_LIGHTING_MATRIX_KEYS; key++) {
            uint16_t code = keycode_at_keymap_location(layer, key / MATRIX_COLS, key % MATRIX_COLS);
            native[layer][key] = code;
            if (!capture_behaviour(code, &behaviour[layer][key])) return false;
            if (combo_matches(code, layer, 255u)) key_origin_bitmap_add_keypos(combo_inputs[layer], (keypos_t){.row = key / MATRIX_COLS, .col = key % MATRIX_COLS});
        }
    }
    // A split/VIA callback may preempt a read and invalidate the source.
    // The caller retains the previous rendered pixels until a settled rebuild.
    if (generation != noah_lighting_keymap_generation()) return false;
    cached_generation = generation;
    ready = true;
    return true;
}

uint8_t noah_lighting_shown_layer(const noah_lighting_scene_t *scene, const noah_lighting_subject_t *subject) {
    if (subject->thing_kind == NOAH_LIGHTING_THING_LAYER && subject->thing < LAYER_COUNT) return subject->thing;
#    if NOAH_LIGHTING_FACT_LAYER_ACTIVE
    uint16_t layers = (uint16_t)(scene->facts->layer_active[0] | (uint16_t)scene->facts->layer_active[1] << 8u) | 1u;
    for (uint8_t layer = LAYER_COUNT; layer-- > 0u;) if (layers & (UINT16_C(1) << layer)) return layer;
#    else
    (void)scene;
    return get_highest_layer(layer_state | default_layer_state | 1u);
#    endif
    return 0u;
}

bool noah_lighting_keymap_identity(uint8_t layer, uint8_t key, uint8_t *kind, uint8_t *id) {
    if (!ready || layer >= LAYER_COUNT || key >= NOAH_LIGHTING_MATRIX_KEYS) return false;
    uint16_t code = native[layer][key];
    *kind = NOAH_LIGHTING_THING_LAYER;
    if (IS_QK_MOMENTARY(code)) *id = QK_MOMENTARY_GET_LAYER(code);
    else if (IS_QK_TO(code)) *id = QK_TO_GET_LAYER(code);
    else if (IS_QK_TOGGLE_LAYER(code)) *id = QK_TOGGLE_LAYER_GET_LAYER(code);
    else if (IS_QK_ONE_SHOT_LAYER(code)) *id = QK_ONE_SHOT_LAYER_GET_LAYER(code);
    else if (IS_QK_LAYER_TAP_TOGGLE(code)) *id = QK_LAYER_TAP_TOGGLE_GET_LAYER(code);
    else if (IS_QK_LAYER_MOD(code)) *id = QK_LAYER_MOD_GET_LAYER(code);
    else if (IS_QK_LAYER_TAP(code)) *id = QK_LAYER_TAP_GET_LAYER(code);
    else if (code >= LAYER_LOCK_BASE && code < LAYER_LOCK_BASE + LAYER_COUNT) *id = code - LAYER_LOCK_BASE;
    else if ((code >= PD_SLOT_0 && code < PD_SLOT_0 + 32u) || (code >= PD_SLOT_0_LOCK && code < PD_SLOT_0_LOCK + 32u)) {
        *kind = NOAH_LIGHTING_THING_POINTING_MODE;
        *id = code >= PD_SLOT_0_LOCK ? code - PD_SLOT_0_LOCK : code - PD_SLOT_0;
        return true;
    } else if (IS_QK_MACRO(code)) {
        *kind = NOAH_LIGHTING_THING_MACRO;
        *id = code - QK_MACRO_0;
        return *id < VIA_MACRO_SLOT_COUNT;
    } else return false;
    return *id < LAYER_COUNT;
}

bool noah_lighting_keymap_matches(uint8_t layer, uint8_t key, uint8_t query, uint8_t selector) {
    if (!ready || layer >= LAYER_COUNT || key >= NOAH_LIGHTING_MATRIX_KEYS) return false;
    uint16_t code = native[layer][key];
    // QK_MODS wraps a literal HID usage, not a host character. Other dual-role
    // and function actions are not guessed to emit their low byte.
    uint16_t basic = IS_QK_MODS(code) ? code & 0xffu : code;
    uint8_t kind, id;
    switch (query) {
        case NOAH_LIGHTING_QUERY_MAPPED: return code != KC_NO && code != KC_TRNS;
        case NOAH_LIGHTING_QUERY_TRANSPARENT: return code == KC_TRNS;
        case NOAH_LIGHTING_QUERY_NONE: return code == KC_NO;
        case NOAH_LIGHTING_QUERY_MODIFIER: return (basic >= 0xe0u && basic <= 0xe7u) || IS_QK_MODS(code) || IS_QK_ONE_SHOT_MOD(code) || IS_QK_MOD_TAP(code) || IS_QK_LAYER_MOD(code);
        case NOAH_LIGHTING_QUERY_LETTER: return basic >= 0x04u && basic <= 0x1du;
        case NOAH_LIGHTING_QUERY_NUMBER: return (basic >= 0x1eu && basic <= 0x27u) || (basic >= 0x59u && basic <= 0x62u);
        case NOAH_LIGHTING_QUERY_SYMBOL: return (basic >= 0x2du && basic <= 0x38u) || (basic >= 0x54u && basic <= 0x57u) || basic == 0x64u || basic == 0x67u || basic == 0x85u;
        case NOAH_LIGHTING_QUERY_NAVIGATION: return (basic >= 0x49u && basic <= 0x52u) || basic == 0x2au || basic == 0x2bu || basic == 0x28u || basic == 0x29u || basic == 0x58u;
        case NOAH_LIGHTING_QUERY_MEDIA: return basic >= 0xa5u && basic <= 0xbeu;
        case NOAH_LIGHTING_QUERY_MOUSE: return basic >= 0xcdu && basic <= 0xdfu;
        case NOAH_LIGHTING_QUERY_CUSTOM: return NOAH_KEYCODE_IS_CUSTOM_KEY(code);
        case NOAH_LIGHTING_QUERY_BEHAVIOUR: return behaviour[layer][key] && (selector == 255u || behaviour[layer][key] == selector + 1u);
        case NOAH_LIGHTING_QUERY_LAYER:
        case NOAH_LIGHTING_QUERY_MODE:
        case NOAH_LIGHTING_QUERY_MACRO:
            return noah_lighting_keymap_identity(layer, key, &kind, &id) && kind == (query == NOAH_LIGHTING_QUERY_LAYER ? NOAH_LIGHTING_THING_LAYER : query == NOAH_LIGHTING_QUERY_MODE ? NOAH_LIGHTING_THING_POINTING_MODE : NOAH_LIGHTING_THING_MACRO) && (selector == 255u || id == selector);
        case NOAH_LIGHTING_QUERY_COMBO:
            if (selector == 255u) return key_origin_bitmap_has_keypos(combo_inputs[layer], (keypos_t){.row = key / MATRIX_COLS, .col = key % MATRIX_COLS});
            return combo_matches(code, layer, selector);
        default: return false;
    }
}
static bool combo_matches(uint16_t code, uint8_t layer, uint8_t selector) {
#    ifdef COMBO_ENABLE
    uint16_t first = selector == 255u ? 0u : selector;
    uint16_t end = selector == 255u ? noah_effective_combo_count() : first + 1u;
    for (uint16_t row = first; row < end; row++) {
        if (!noah_effective_combo_enabled(row) || !(noah_effective_combo_allowed_layers(row) & (UINT32_C(1) << layer))) continue;
        combo_t *combo = noah_effective_combo_get(row);
        if (!combo) continue;
        for (uint8_t input = 0u; input < NOAH_PROFILE_COMBO_V1_MAX_INPUTS; input++) {
            uint16_t member = pgm_read_word(&combo->keys[input]);
            if (member == COMBO_END) break;
#        ifdef NOAH_LIGHTING_KEYMAP_TEST_BACKEND
            comparisons++;
#        endif
            if (member == code) return true;
        }
    }
#    else
    (void)code;
    (void)layer;
    (void)selector;
#    endif
    return false;
}
#endif
