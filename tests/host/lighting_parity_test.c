#include "users/noah/lib/pointing/runtime/pd_mode_snapshot_internal.h"
// Lighting parity: the retired stage renderer (tests/host/legacy_rgb/, the
// oracle) and the lighting rule engine render the same corpus and must write
// identical frames. Compiled twice: with LIGHTING_PARITY_ORACLE the old
// renderer reads each configuration's version-4 domain; without it the new
// rgb_runtime.c renders that domain converted to Standard lighting, collecting
// its facts through the real fact modules. Both read the same fake runtime:
// layers, keymap, preview, pointing mode, combos, key-behaviour maps,
// auto-mouse and the matrix effect. run_lighting_parity_tests.sh compares the
// two outputs byte for byte.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qmk_stub.h"
#include "rgb_matrix.h"
#include "ws2812.h"
#include "users/noah/lib/key/runtime/feedback.h"
#include "users/noah/lib/pointing/defs/pd_mode_flags.h"
#include "users/noah/lib/profile/schema/profile_rgb_v1.h"
#include "users/noah/lib/rgb/core/rgb_runtime.h"
#include "users/noah/lib/state/diagnostics/runtime_diag.h"

#ifndef LIGHTING_PARITY_ORACLE
#    include "users/noah/lib/profile/runtime/effective_rgb_runtime.h"
#    include "users/noah/lib/profile/schema/profile_lighting_v1.h"
#endif

bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max);

// ─── The situation every fake reads ─────────────────────────────────────────

typedef struct {
    uint8_t mapped[LAYER_COUNT][KEY_ORIGIN_BITMAP_SIZE];
    uint8_t preview;
    uint8_t mode;
    uint8_t mode_sides;
    bool    mode_keyed;
    uint8_t mode_keys[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t underlay[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t overlay[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t semantic[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t branch[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    uint8_t visible[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t owners[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
    bool    auto_mouse_enabled;
    bool    mode_locked;
    uint16_t auto_mouse_elapsed;
    bool    diag;
} situation_t;

static situation_t   situation;
layer_state_t        layer_state;
layer_state_t        default_layer_state;
ws2812_led_t         ws2812_leds[WS2812_LED_COUNT];
led_config_t         g_led_config;
static rgb_t         written[RGB_MATRIX_LED_COUNT];
static bool          written_mask[RGB_MATRIX_LED_COUNT];
static bool          is_left;
static uint32_t      clock_ms;
static const int8_t  key_leds[MATRIX_ROWS * MATRIX_COLS] = {
    0, 7, 8, 15, 16, 20, 1, 6, 9, 14, 17, 21, 2, 5, 10, 13, 18, 22, 3, 4, 11, 12, 19, 23, -1, 28, 24, 26, 27, 25,
    29, 36, 37, 44, 45, 49, 30, 35, 38, 43, 46, 50, 31, 34, 39, 42, 47, 51, 32, 33, 40, 41, 48, 52, -1, 53, 55, 54, -1, -1,
};

// ─── QMK stand-ins ──────────────────────────────────────────────────────────

rgb_t hsv_to_rgb(hsv_t hsv) {
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
            return (rgb_t){.r = (uint8_t)v, .g = t, .b = p};
        case 1:
            return (rgb_t){.r = q, .g = (uint8_t)v, .b = p};
        case 2:
            return (rgb_t){.r = p, .g = (uint8_t)v, .b = t};
        case 3:
            return (rgb_t){.r = p, .g = q, .b = (uint8_t)v};
        case 4:
            return (rgb_t){.r = t, .g = p, .b = (uint8_t)v};
        default:
            return (rgb_t){.r = (uint8_t)v, .g = p, .b = q};
    }
}

void rgb_matrix_set_color(int index, uint8_t red, uint8_t green, uint8_t blue) {
    assert(index >= 0 && index < RGB_MATRIX_LED_COUNT);
    written[index]      = (rgb_t){.r = red, .g = green, .b = blue};
    written_mask[index] = true;
}

int rgb_matrix_led_index(int index) {
    return index;
}

uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t column, uint8_t *led_i) {
    int8_t led = key_leds[row * MATRIX_COLS + column];
    if (led < 0) return 0u;
    led_i[0] = (uint8_t)led;
    return 1u;
}

struct rgb_matrix_limits_t rgb_matrix_get_limits(uint8_t iter) {
    struct rgb_matrix_limits_t limits;
    limits.led_min_index = (uint8_t)(RGB_MATRIX_LED_PROCESS_LIMIT * iter);
    limits.led_max_index = (uint8_t)(limits.led_min_index + RGB_MATRIX_LED_PROCESS_LIMIT);
    if (limits.led_max_index > RGB_MATRIX_LED_COUNT) limits.led_max_index = RGB_MATRIX_LED_COUNT;
    return limits;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    keypos_t key = {.row = row, .col = column};
    return key_origin_bitmap_has_keypos(situation.mapped[layer_num], key) ? KC_A : (column & 1u ? KC_TRNS : KC_NO);
}

bool is_keyboard_master(void) {
    return true;
}

bool is_keyboard_left(void) {
    return is_left;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    if (!state) return layer == 0u;
    return (state & ((layer_state_t)1u << layer)) != 0u;
}

uint32_t timer_read32(void) {
    return clock_ms;
}

uint16_t timer_read(void) {
    return (uint16_t)clock_ms;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)((uint16_t)clock_ms - last);
}

bool     get_auto_mouse_enable(void) { return situation.auto_mouse_enabled; }
uint8_t  get_auto_mouse_layer(void) { return 4u; }
uint16_t auto_mouse_get_time_elapsed(void) { return situation.auto_mouse_elapsed; }
uint16_t auto_mouse_get_time_elapsed_at(uint16_t now) { (void)now; return situation.auto_mouse_elapsed; }
bool     pd_any_display_mode_locked(void) { return situation.mode_locked; }

bool noah_runtime_diag_indicator_active(void) {
    return situation.diag;
}
void noah_runtime_diag_scope_enter(noah_runtime_diag_stage_t stage) {
    (void)stage;
}
void noah_runtime_diag_scope_leave(void) {}
void noah_rgb_validate_config(void) {}

// ─── The key runtime's feedback, as the USB half produces it ────────────────

void key_feedback_semantic_map(uint8_t *out_map) {
    memcpy(out_map, situation.semantic, KEY_FEEDBACK_SEMANTIC_MAP_SIZE);
}

void key_feedback_tap_branch_map(uint8_t *out_map) {
    memcpy(out_map, situation.branch, KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE);
}

void key_feedback_broad_owner_map(uint8_t *out_map) {
    memcpy(out_map, situation.owners, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE);
}

void key_feedback_flash_visibility_bitmap_for_semantic_map(const uint8_t *semantic_map, uint8_t *out_bitmap) {
    key_origin_bitmap_clear(out_bitmap);
    for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
            keypos_t key = {.row = row, .col = col};
            if (key_feedback_semantic_is_flashing(key_feedback_semantic_map_get(semantic_map, key)) && key_origin_bitmap_has_keypos(situation.visible, key)) key_origin_bitmap_add_keypos(out_bitmap, key);
        }
    }
}

uint8_t key_feedback_preview_layer(void) {
    return situation.preview;
}

void combo_feedback_bitmaps(uint8_t *out_underlay_bitmap, uint8_t *out_overlay_bitmap) {
    key_origin_bitmap_copy(out_underlay_bitmap, situation.underlay);
    key_origin_bitmap_copy(out_overlay_bitmap, situation.overlay);
}

pd_mode_snapshot_t pd_mode_snapshot_with_owner_bitmap(uint8_t *out_owner_bitmap, bool *out_has_owner_keys) {
    pd_mode_snapshot_t snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.display.active_index = situation.mode;
    snapshot.display.owner_sides  = situation.mode_sides;
    snapshot.local                = snapshot.display;
    key_origin_bitmap_clear(out_owner_bitmap);
    if (situation.mode_keyed) key_origin_bitmap_copy(out_owner_bitmap, situation.mode_keys);
    if (out_has_owner_keys) *out_has_owner_keys = situation.mode_keyed;
    return snapshot;
}

// Adapt the fixture's existing full view; production uses compact real queries.
void pd_mode_local_owner_view(pd_mode_owner_view_t *out, uint8_t *owners) {
    bool has_keys;
    pd_mode_snapshot_t snapshot = pd_mode_snapshot_with_owner_bitmap(owners, &has_keys);
    *out = (pd_mode_owner_view_t){.active_index = snapshot.local.active_index, .locked_index = snapshot.local.locked_index, .owner_sides = snapshot.local.owner_sides, .has_owner_keys = has_keys};
}
void pd_mode_display_owner_view(pd_mode_owner_view_t *out, uint8_t *owners) {
    bool has_keys;
    pd_mode_snapshot_t snapshot = pd_mode_snapshot_with_owner_bitmap(owners, &has_keys);
    *out = (pd_mode_owner_view_t){.active_index = snapshot.display.active_index, .locked_index = snapshot.display.locked_index, .owner_sides = snapshot.display.owner_sides, .has_owner_keys = has_keys};
}

// ─── Configurations ─────────────────────────────────────────────────────────

static uint8_t                    v4_bytes[2048];
static size_t                     v4_length;
static noah_profile_rgb_v1_view_t v4_view;

#ifdef LIGHTING_PARITY_ORACLE
const noah_profile_rgb_v1_view_t *legacy_rgb_v4_view(void) {
    return &v4_view;
}

key_feedback_tap_commit_mode_t key_feedback_tap_commit_mode(void);
#else
static noah_profile_lighting_v1_program_t program;
static uint32_t                           program_generation;
static uint8_t                            v5_bytes[4096];

const noah_profile_lighting_v1_program_t *noah_effective_lighting_program(uint32_t *generation) {
    if (generation) *generation = program_generation;
    return &program;
}
uint32_t noah_effective_lighting_generation(void) { return program_generation; }
#endif

static void load_configuration(const char *hex) {
    noah_profile_rgb_v1_limits_t limits = noah_profile_rgb_v1_default_limits();

    v4_length = strlen(hex) / 2u;
    assert(v4_length <= sizeof(v4_bytes));
    for (size_t i = 0; i < v4_length; i++) {
        unsigned value;
        assert(sscanf(hex + 2 * i, "%2x", &value) == 1);
        v4_bytes[i] = (uint8_t)value;
    }
    limits.logical_layer_count    = LAYER_COUNT;
    limits.maximum_brightness     = 200u;
    limits.tap_branch_color_count = KEY_BEHAVIOR_MAX_TAP_COUNT - 1u;
    assert(noah_profile_rgb_v1_decode(v4_bytes, v4_length, &limits, &v4_view, NULL) == NOAH_PROFILE_RGB_V1_OK);
#ifndef LIGHTING_PARITY_ORACLE
    {
        noah_profile_lighting_v1_limits_t target = noah_profile_lighting_v1_default_limits();
        noah_profile_lighting_v1_view_t   view;
        size_t                            length;
        target.logical_layer_count = LAYER_COUNT;
        target.maximum_brightness  = 200u;
        assert(noah_profile_lighting_v1_convert_rgb_v4(&v4_view, &target, v5_bytes, sizeof(v5_bytes), &length) == NOAH_PROFILE_LIGHTING_V1_OK);
        assert(noah_profile_lighting_v1_decode(v5_bytes, length, &target, &view, NULL) == NOAH_PROFILE_LIGHTING_V1_OK);
        assert(noah_profile_lighting_v1_program_load(&view, &program));
        program_generation++;
    }
#endif
}

// ─── The corpus ─────────────────────────────────────────────────────────────

static uint32_t rng_state;

static uint32_t rng(void) {
    rng_state ^= rng_state << 13u;
    rng_state ^= rng_state >> 17u;
    rng_state ^= rng_state << 5u;
    return rng_state;
}

static bool chance(unsigned percent) {
    return rng() % 100u < percent;
}

static keypos_t random_key(void) {
    unsigned index = rng() % (MATRIX_ROWS * MATRIX_COLS);
    return (keypos_t){.row = (uint8_t)(index / MATRIX_COLS), .col = (uint8_t)(index % MATRIX_COLS)};
}

static void random_keys(uint8_t *bitmap, unsigned max) {
    unsigned count = rng() % (max + 1u);
    key_origin_bitmap_clear(bitmap);
    for (unsigned i = 0; i < count; i++) key_origin_bitmap_add_keypos(bitmap, random_key());
}

// One fixed keymap for the run: mapped keys differ per layer, and every layer
// has transparent and empty positions.
static void build_keymap(void) {
    rng_state = 0x5eed1234u;
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        key_origin_bitmap_clear(situation.mapped[layer]);
        for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
                if (chance(layer == 0u ? 90u : 35u)) key_origin_bitmap_add_keypos(situation.mapped[layer], (keypos_t){.row = row, .col = col});
            }
        }
    }
}

typedef struct {
    unsigned preview, mode, combos, behaviour, flashing_dark, fade, diag, repeats;
} coverage_t;

static coverage_t coverage;

static void generate(unsigned index) {
    uint8_t keys_in_gesture[8];
    uint8_t gesture_count = 0u;

    rng_state = 0x9e3779b9u ^ (index * 2654435761u);
    rng();
    // Every seventh situation keeps the facts and changes only the matrix
    // effect and the clock: no re-resolve, but what composes over the effect
    // must follow it.
    if (index % 7u == 6u) {
        for (uint8_t led = 0u; led < WS2812_LED_COUNT; led++) ws2812_leds[led] = (ws2812_led_t){.r = (uint8_t)rng(), .g = (uint8_t)rng(), .b = (uint8_t)rng()};
        clock_ms += 1u + rng() % 50u;
        coverage.repeats++;
        return;
    }
    layer_state = 0u;
    for (uint8_t layer = 1u; layer < LAYER_COUNT; layer++) {
        if (chance(layer < 6u ? 30u : 8u)) layer_state |= (layer_state_t)1u << layer;
    }
    situation.preview    = chance(20u) ? (uint8_t)(rng() % LAYER_COUNT) : UINT8_MAX;
    situation.mode       = chance(40u) ? (uint8_t)(rng() % PD_MODE_COUNT) : PD_MODE_COUNT;
    situation.mode_sides = (uint8_t)(rng() % 4u);
    situation.mode_keyed = chance(50u);
    random_keys(situation.mode_keys, 3u);
    if (!key_origin_bitmap_has_any(situation.mode_keys)) situation.mode_keyed = false;
    if (chance(35u)) {
        random_keys(situation.underlay, 3u);
    } else {
        key_origin_bitmap_clear(situation.underlay);
    }
    if (chance(35u)) {
        random_keys(situation.overlay, 3u);
    } else {
        key_origin_bitmap_clear(situation.overlay);
    }
    key_feedback_semantic_map_clear(situation.semantic);
    key_feedback_tap_branch_map_clear(situation.branch);
    key_origin_bitmap_clear(situation.visible);
    key_feedback_broad_owner_map_clear(situation.owners);
    for (unsigned count = rng() % 5u; count-- > 0u;) {
        keypos_t                key      = random_key();
        key_feedback_semantic_t semantic = (key_feedback_semantic_t)(1u + rng() % KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED);
        key_feedback_semantic_map_set(situation.semantic, key, semantic);
        if (semantic == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING || chance(10u)) key_feedback_tap_branch_map_set(situation.branch, key, (uint8_t)(rng() % 9u));
        if (chance(50u)) key_origin_bitmap_add_keypos(situation.visible, key);
        keys_in_gesture[gesture_count++] = (uint8_t)key_origin_keypos_index(key);
    }
    for (uint8_t slot = 0u; slot < KEY_FEEDBACK_BROAD_OWNER_COUNT; slot++) {
        unsigned roll = rng() % 100u;
        if (roll < 60u && gesture_count) {
            uint8_t index = keys_in_gesture[rng() % gesture_count];
            key_feedback_broad_owner_map_set(situation.owners, (key_feedback_broad_owner_slot_t)slot, (keypos_t){.row = (uint8_t)(index / MATRIX_COLS), .col = (uint8_t)(index % MATRIX_COLS)});
        } else if (roll < 70u) {
            key_feedback_broad_owner_map_set(situation.owners, (key_feedback_broad_owner_slot_t)slot, random_key());
        }
    }
    situation.auto_mouse_enabled = chance(60u);
    situation.auto_mouse_elapsed = (uint16_t)(rng() % 1100u);
    situation.mode_locked        = chance(10u);
    if (situation.auto_mouse_enabled && chance(60u)) layer_state |= (layer_state_t)1u << 4u;
    situation.diag = chance(2u);
    for (uint8_t led = 0u; led < WS2812_LED_COUNT; led++) {
        ws2812_leds[led] = (ws2812_led_t){.r = (uint8_t)rng(), .g = (uint8_t)rng(), .b = (uint8_t)rng()};
    }
    clock_ms += 1u + rng() % 50u;
    coverage.preview += situation.preview < LAYER_COUNT;
    coverage.mode += situation.mode < PD_MODE_COUNT;
    coverage.combos += key_origin_bitmap_has_any(situation.underlay) || key_origin_bitmap_has_any(situation.overlay);
    coverage.behaviour += gesture_count > 0u;
    coverage.diag += situation.diag;
    coverage.fade += situation.auto_mouse_enabled && !situation.mode_locked && (layer_state & (1u << 4u)) && situation.auto_mouse_elapsed > 300u;
    for (uint8_t i = 0u; i < gesture_count; i++) {
        keypos_t key = {.row = (uint8_t)(keys_in_gesture[i] / MATRIX_COLS), .col = (uint8_t)(keys_in_gesture[i] % MATRIX_COLS)};
        if (key_feedback_semantic_is_flashing(key_feedback_semantic_map_get(situation.semantic, key)) && !key_origin_bitmap_has_keypos(situation.visible, key)) {
            coverage.flashing_dark++;
            break;
        }
    }
}

// Renders both halves chunk by chunk, as QMK does, and prints what was written.
static void render(unsigned configuration, unsigned index) {
    for (uint8_t half = 0u; half < 2u; half++) {
        bool painted = false;
        is_left      = half == 0u;
        memset(written_mask, 0, sizeof(written_mask));
        for (uint8_t iter = 0u;; iter++) {
            struct rgb_matrix_limits_t limits = rgb_matrix_get_limits(iter);
            if (limits.led_min_index >= RGB_MATRIX_LED_COUNT) break;
            painted |= noah_rgb_matrix_indicators_advanced_user(limits.led_min_index, limits.led_max_index);
        }
        printf("%u %u %u %u", configuration, index, half, painted ? 1u : 0u);
        for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
            if (written_mask[led]) {
                printf(" %02x%02x%02x", written[led].r, written[led].g, written[led].b);
            } else {
                printf(" -");
            }
        }
        printf("\n");
    }
}

int main(int argc, char **argv) {
    static char hex[8192];
    unsigned    configuration = 0u;
    unsigned    count;

    assert(argc == 3);
    count  = (unsigned)strtoul(argv[2], NULL, 10);
    FILE *file = fopen(argv[1], "r");
    assert(file);
    build_keymap();
    while (fscanf(file, "%8191s", hex) == 1) {
        load_configuration(hex);
        noah_rgb_runtime_post_init();
#ifdef LIGHTING_PARITY_ORACLE
        printf("tap %u %u\n", configuration, (unsigned)key_feedback_tap_commit_mode());
#else
        printf("tap %u %u\n", configuration, (unsigned)key_feedback_tap_commit_mode());
#endif
        for (unsigned index = 0u; index < count; index++) {
            generate(index);
            render(configuration, index);
        }
        configuration++;
    }
    fclose(file);
    fprintf(stderr, "coverage: %u preview, %u pointing mode, %u combo, %u behaviour (%u with a dark flash), %u auto-mouse fade, %u diagnostics, %u effect-only repeats\n", coverage.preview, coverage.mode, coverage.combos, coverage.behaviour, coverage.flashing_dark, coverage.fade, coverage.diag, coverage.repeats);
    return 0;
}
