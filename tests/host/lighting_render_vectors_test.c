// Lighting render vectors: every per-entry render vector in
// tests/fixtures/lighting_vectors_v1.json, rendered by the firmware engine on
// the Charybdis 4x6 geometry. Each must match the reference model colour for
// colour. run_lighting_render_vectors_tests.sh flattens the JSON into lines.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/rgb/lighting/lighting_engine.h"
#include "users/noah/lib/pointing/defs/pd_mode_flags.h"
#include "users/noah/lib/profile/runtime/effective_settings_runtime.h"

// ─── QMK and runtime stand-ins ──────────────────────────────────────────────

layer_state_t layer_state;
static int8_t  key_leds[MATRIX_ROWS * MATRIX_COLS];
static uint8_t mapped[LAYER_COUNT][KEY_ORIGIN_BITMAP_SIZE];
static rgb_t   effect[RGB_MATRIX_LED_COUNT];
static bool    effect_known[RGB_MATRIX_LED_COUNT];
static uint16_t fade_progress;
static uint32_t fade_timeout = 900u, fade_dead_time = 300u;
static uint8_t  fade_layer = 0xffu;

rgb_t hsv_to_rgb(hsv_t hsv) {
    rgb_t    rgb;
    uint8_t  region, remainder, p, q, t;
    uint16_t h = hsv.h, s = hsv.s, v = hsv.v;

    if (hsv.s == 0) return (rgb_t){.r = hsv.v, .g = hsv.v, .b = hsv.v};
    region    = (uint8_t)(h * 6 / 255);
    remainder = (uint8_t)((h * 2 - region * 85) * 3);
    p         = (uint8_t)((v * (255 - s)) >> 8);
    q         = (uint8_t)((v * (255 - ((s * remainder) >> 8))) >> 8);
    t         = (uint8_t)((v * (255 - ((s * (255 - remainder)) >> 8))) >> 8);
    switch (region) {
        case 6:
        case 0:
            rgb = (rgb_t){.r = (uint8_t)v, .g = t, .b = p};
            break;
        case 1:
            rgb = (rgb_t){.r = q, .g = (uint8_t)v, .b = p};
            break;
        case 2:
            rgb = (rgb_t){.r = p, .g = (uint8_t)v, .b = t};
            break;
        case 3:
            rgb = (rgb_t){.r = p, .g = q, .b = (uint8_t)v};
            break;
        case 4:
            rgb = (rgb_t){.r = t, .g = p, .b = (uint8_t)v};
            break;
        default:
            rgb = (rgb_t){.r = (uint8_t)v, .g = p, .b = q};
            break;
    }
    return rgb;
}

uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t column, uint8_t *led_i) {
    int8_t led = key_leds[row * MATRIX_COLS + column];
    if (led < 0) return 0u;
    led_i[0] = (uint8_t)led;
    return 1u;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    keypos_t key = {.row = row, .col = column};
    return key_origin_bitmap_has_keypos(mapped[layer_num], key) ? KC_A : KC_NO;
}

bool noah_lighting_effect_colour(uint8_t led, rgb_t *colour) {
    if (!effect_known[led]) return false;
    *colour = effect[led];
    return true;
}

uint32_t noah_setting(uint8_t id, uint32_t fallback) {
    if (id == NOAH_SETTING_AUTO_MOUSE_TIMEOUT) return fade_timeout;
    if (id == NOAH_SETTING_AUTO_MOUSE_DEAD_TIME) return fade_dead_time;
    return fallback;
}

uint16_t automouse_rgb_current_progress(void) {
    uint16_t span = (uint16_t)(fade_timeout - fade_dead_time);
    return fade_progress > span ? span : fade_progress;
}

uint8_t get_auto_mouse_layer(void) {
    return fade_layer;
}

// Collection is the USB half's job; these vectors set the facts directly.
void    key_feedback_semantic_map(uint8_t *out_map) { key_feedback_semantic_map_clear(out_map); }
void    key_feedback_tap_branch_map(uint8_t *out_map) { key_feedback_tap_branch_map_clear(out_map); }
void    key_feedback_broad_owner_map(uint8_t *out_map) { key_feedback_broad_owner_map_clear(out_map); }
void    key_feedback_flash_visibility_bitmap_for_semantic_map(const uint8_t *semantic_map, uint8_t *out_bitmap) { (void)semantic_map; key_origin_bitmap_clear(out_bitmap); }
uint8_t key_feedback_preview_layer(void) { return UINT8_MAX; }
void    combo_feedback_bitmaps(uint8_t *underlay, uint8_t *overlay) { key_origin_bitmap_clear(underlay); key_origin_bitmap_clear(overlay); }
pd_mode_snapshot_t pd_mode_snapshot_with_owner_bitmap(uint8_t *out_owner_bitmap, bool *out_has_owner_keys) {
    pd_mode_snapshot_t snapshot = {0};
    key_origin_bitmap_clear(out_owner_bitmap);
    *out_has_owner_keys        = false;
    snapshot.display.active_index = PD_MODE_COUNT;
    return snapshot;
}

uint32_t noah_lighting_keymap_generation(void) {
    return 1u;
}

// ─── Vectors ────────────────────────────────────────────────────────────────

static void read_hex(FILE *file, uint8_t *out, size_t length) {
    for (size_t i = 0; i < length; i++) {
        unsigned value;
        assert(fscanf(file, "%2x", &value) == 1);
        out[i] = (uint8_t)value;
    }
}

static keypos_t key_at(unsigned index) {
    return (keypos_t){.row = (uint8_t)(index / MATRIX_COLS), .col = (uint8_t)(index % MATRIX_COLS)};
}

int main(int argc, char **argv) {
    static noah_profile_lighting_v1_program_t program;
    static noah_lighting_facts_t              facts;
    static noah_lighting_frame_t              frame;
    static char                               payload_hex[20000];
    static uint8_t                            payload[10000];
    char                                      word[96], name[96];
    size_t                                    cases = 0, steps = 0;
    noah_profile_lighting_v1_limits_t         limits = noah_profile_lighting_v1_default_limits();

    assert(argc == 2);
    limits.maximum_brightness = 200u;
    FILE *file = fopen(argv[1], "r");
    assert(file);
    assert(fscanf(file, "%95s", word) == 1 && strcmp(word, "GEOMETRY") == 0);
    for (unsigned key = 0; key < MATRIX_ROWS * MATRIX_COLS; key++) {
        int led;
        assert(fscanf(file, "%d", &led) == 1);
        key_leds[key] = (int8_t)led;
    }
    while (fscanf(file, "%95s", word) == 1) {
        if (strcmp(word, "CASE") == 0) {
            noah_profile_lighting_v1_view_t view;
            assert(fscanf(file, "%95s %19999s", name, payload_hex) == 2);
            size_t length = strlen(payload_hex) / 2u;
            for (size_t i = 0; i < length; i++) {
                unsigned value;
                assert(sscanf(payload_hex + 2 * i, "%2x", &value) == 1);
                payload[i] = (uint8_t)value;
            }
            assert(noah_profile_lighting_v1_decode(payload, length, &limits, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
            assert(noah_profile_lighting_v1_program_load(&view, &program));
            noah_lighting_engine_program_changed();
            cases++;
            continue;
        }
        assert(strcmp(word, "STEP") == 0);
        unsigned long now;
        unsigned      layers, preview, mode, sides, keyed, count, owners[KEY_FEEDBACK_BROAD_OWNER_COUNT], fade_values[4];
        uint8_t       mode_keys[KEY_ORIGIN_BITMAP_SIZE];
        memset(&facts, 0, sizeof(facts));
        assert(fscanf(file, "%lu %u %u %u %u %u", &now, &layers, &preview, &mode, &sides, &keyed) == 6);
        read_hex(file, mode_keys, sizeof(mode_keys));
        facts.layer_active[0]     = (uint8_t)layers;
        facts.layer_active[1]     = (uint8_t)(layers >> 8u);
        facts.layer_previewing[0] = (uint8_t)preview;
        facts.pointing_mode_active[0] = (uint8_t)mode;
        facts.pointing_mode_active[1] = (uint8_t)sides;
        facts.pointing_mode_active[2] = (uint8_t)keyed;
        memcpy(&facts.pointing_mode_active[3], mode_keys, sizeof(mode_keys));
        read_hex(file, facts.combos_held, 2u * KEY_ORIGIN_BITMAP_SIZE);
        for (unsigned layer = 0; layer < LAYER_COUNT; layer++) read_hex(file, mapped[layer], KEY_ORIGIN_BITMAP_SIZE);
        {
            uint8_t *slice    = facts.behaviour_state;
            uint8_t *visible  = slice;
            uint8_t *semantic = visible + KEY_ORIGIN_BITMAP_SIZE;
            uint8_t *owner    = semantic + KEY_FEEDBACK_SEMANTIC_MAP_SIZE;
            uint8_t *branch   = owner + KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE;
            key_feedback_broad_owner_map_clear(owner);
            assert(fscanf(file, "%u", &count) == 1);
            for (unsigned entry = 0; entry < count; entry++) {
                unsigned key, state, branch_value, flashing, lit;
                assert(fscanf(file, "%u %u %u %u %u", &key, &state, &branch_value, &flashing, &lit) == 5);
                key_feedback_semantic_t value = state == 0 ? KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING : state == 1 ? KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED : state == 2 ? (flashing ? KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING : KEY_FEEDBACK_SEMANTIC_HOLD_PENDING) : (flashing ? KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING : KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY);
                key_feedback_semantic_map_set(semantic, key_at(key), value);
                if (state == 0) key_feedback_tap_branch_map_set(branch, key_at(key), (uint8_t)branch_value);
                if (lit) key_origin_bitmap_add_keypos(visible, key_at(key));
            }
            for (unsigned slot = 0; slot < KEY_FEEDBACK_BROAD_OWNER_COUNT; slot++) {
                assert(fscanf(file, "%u", &owners[slot]) == 1);
                if (owners[slot] != 255u) key_feedback_broad_owner_map_set(owner, (key_feedback_broad_owner_slot_t)slot, key_at(owners[slot]));
            }
        }
        memset(effect_known, 0, sizeof(effect_known));
        assert(fscanf(file, "%u", &count) == 1);
        for (unsigned entry = 0; entry < count; entry++) {
            unsigned led, colour;
            assert(fscanf(file, "%u %6x", &led, &colour) == 2);
            effect[led]       = (rgb_t){.r = (uint8_t)(colour >> 16), .g = (uint8_t)(colour >> 8), .b = (uint8_t)colour};
            effect_known[led] = true;
        }
        for (unsigned index = 0; index < 4; index++) assert(fscanf(file, "%u", &fade_values[index]) == 1);
        fade_progress  = (uint16_t)fade_values[0];
        fade_timeout   = fade_values[1];
        fade_dead_time = fade_values[2];
        fade_layer     = (uint8_t)fade_values[3];

        noah_lighting_scene_t        scene = {.program = &program, .facts = &facts, .keymap_generation = 1u};
        noah_lighting_resolve_info_t info;
        memset(&frame, 0, sizeof(frame));
        noah_lighting_resolve(&scene, NOAH_LIGHTING_ALL_LEDS, (uint32_t)now, true, &frame, &info);
        for (unsigned led = 0; led < RGB_MATRIX_LED_COUNT; led++) {
            char expected[16], actual[16];
            assert(fscanf(file, "%15s", expected) == 1);
            if (frame.painted & NOAH_LIGHTING_LED_BIT(led)) {
                snprintf(actual, sizeof(actual), "%02x%02x%02x", frame.colour[led].r, frame.colour[led].g, frame.colour[led].b);
            } else {
                snprintf(actual, sizeof(actual), "-");
            }
            if (strcmp(expected, actual) != 0) {
                fprintf(stderr, "lighting render vector %s step %zu LED %u: expected %s, got %s\n", name, steps, led, expected, actual);
                return 1;
            }
        }
        steps++;
    }
    fclose(file);
    assert(cases >= 30 && steps >= 60);
    printf("lighting render vectors: %zu cases, %zu steps passed\n", cases, steps);
    return 0;
}
