// The replacement feedback stage over the compiled lighting and a live
// profile: which LEDs a flash paints for each semantic, locality and accent.
// Keys are a 2 x 4 matrix, row 0 the left half (LEDs 0..3), row 1 the right
// half (LEDs 4..7); a key's LED is its matrix index.
#include QMK_KEYBOARD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/rgb/core/rgb_config_helpers.h"
#include "users/noah/lib/rgb/stages/rgb_autocorrect_feedback_stage.h"

const layer_color_config_t     layer_colors[LAYER_COUNT] = {0};
const layer_led_group_t *const layer_led_groups          = NULL;
const uint8_t                  layer_led_group_count     = 0u;

// The compiled lighting: an undo that paints nothing by itself, an accent
// that follows every semantic and one of the undo's own.
const autocorrect_feedback_color_config_t autocorrect_feedback_colors = {
    .text_color    = HSV(11, 12, 13),
    .macro_color   = HSV(21, 22, 23),
    .undo_color    = HSV(0, 0, 0),
    .leading_color = HSV(91, 92, 93),
    .locality      = RGB_KEYS_ONLY,
};

static const autocorrect_feedback_led_group_t autocorrect_feedback_led_groups_data[] = {
    {.semantic = AUTOCORRECT_FEEDBACK_GROUP_ALL, .color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP(7)},
    {.semantic = AUTOCORRECT_FEEDBACK_GROUP_UNDO, .color = HSV(201, 202, 203), .led_group = RGB_LED_GROUP(6)},
};
EXPORT_AUTOCORRECT_FEEDBACK_LED_GROUPS(autocorrect_feedback_led_groups_data);

// Room for the compiled encoding of this test's lighting tables.
EXPORT_RGB_COMPILED_CACHE(0u, 0u, 0u, 0u, 0u, ARRAY_SIZE(autocorrect_feedback_led_groups_data));

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

rgb_t hsv_to_rgb(hsv_t hsv) {
    return (rgb_t){.r = hsv.h, .g = hsv.s, .b = hsv.v};
}

uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t column, uint8_t *led_i) {
    if (!(led_i && row < MATRIX_ROWS && column < MATRIX_COLS)) {
        return 0u;
    }
    led_i[0] = (uint8_t)(row * MATRIX_COLS + column);
    return 1u;
}

void rgb_runtime_frame_clear(rgb_runtime_frame_t *frame, uint8_t led_min, uint8_t led_max) {
    for (uint8_t led = led_min; led < led_max; led++) {
        frame->colors[led]  = (rgb_t){0};
        frame->painted[led] = false;
    }
}

static const rgb_t text_rgb  = {.r = 31u, .g = 32u, .b = 33u};
static const rgb_t macro_rgb = {.r = 41u, .g = 42u, .b = 43u};
static const rgb_t undo_rgb  = {.r = 51u, .g = 52u, .b = 53u};
static const rgb_t lead_rgb  = {.r = 101u, .g = 102u, .b = 103u};

// What a frame shows: `mask` has a bit for each painted LED, all in `color`.
static void check_frame(const rgb_runtime_frame_t *frame, uint8_t mask, rgb_t color) {
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        bool expected = (mask & (1u << led)) != 0u;

        CHECK(frame->painted[led] == expected);
        if (expected) {
            CHECK(frame->colors[led].r == color.r && frame->colors[led].g == color.g && frame->colors[led].b == color.b);
        }
    }
}

static noah_autocorrect_feedback_t feedback_on(noah_autocorrect_feedback_kind_t kind, uint8_t key_mask) {
    noah_autocorrect_feedback_t feedback = {.kind = (uint8_t)kind};

    for (uint8_t key = 0u; key < MATRIX_ROWS * MATRIX_COLS; key++) {
        if (key_mask & (1u << key)) key_origin_bitmap_add_keypos(feedback.keys, (keypos_t){.row = (uint8_t)(key / MATRIX_COLS), .col = (uint8_t)(key % MATRIX_COLS)});
    }
    return feedback;
}

// ── A live profile ──────────────────────────────────────────────────────────

typedef struct {
    uint8_t selector;
    hsv_t   color;
    uint8_t led_mask;
} live_row_t;

enum { LIVE_PAYLOAD_MAX = 256 };

// A lighting domain with the layer stage and this one: one LED set per
// distinct row mask, the four colours, a locality and the rows.
static size_t live_payload(uint8_t *payload, uint16_t stage_mask, hsv_t text, hsv_t macro, hsv_t undo, hsv_t lead, uint8_t locality, const live_row_t *rows, uint8_t row_count) {
    uint8_t masks[8];
    uint8_t groups = 0u;
    size_t  offset;

    // LED sets ascend by their bitmap bytes, each once.
    for (uint16_t mask = 0u; mask <= UINT8_MAX; mask++) {
        for (uint8_t row = 0u; row < row_count; row++) {
            if (rows[row].led_mask == mask && (groups == 0u || masks[groups - 1u] != mask)) masks[groups++] = (uint8_t)mask;
        }
    }
    memset(payload, 0, LIVE_PAYLOAD_MAX);
    payload[0]  = NOAH_PROFILE_RGB_V1_FORMAT_VERSION;
    payload[2]  = (uint8_t)stage_mask;
    payload[4]  = groups;
    payload[5]  = LAYER_COUNT;
    payload[12] = NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT;
    payload[13] = NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE;
    payload[14] = row_count;
    offset      = NOAH_PROFILE_RGB_V1_HEADER_SIZE;
    for (uint8_t group = 0u; group < groups; group++) {
        payload[offset]      = group;
        payload[offset + 1u] = masks[group];
        offset += NOAH_PROFILE_RGB_V1_GROUP_RECORD_SIZE;
    }
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        payload[offset] = layer;
        offset += NOAH_PROFILE_RGB_V1_LAYER_COLOR_RECORD_SIZE;
    }
    offset += NOAH_PROFILE_RGB_V1_AUTOMOUSE_RECORD_SIZE + NOAH_PROFILE_RGB_V1_COMBO_RECORD_SIZE + NOAH_PROFILE_RGB_V1_KEY_FEEDBACK_SIZE;
    memcpy(&payload[offset], (const uint8_t[]){text.h, text.s, text.v, macro.h, macro.s, macro.v, undo.h, undo.s, undo.v, lead.h, lead.s, lead.v, locality}, NOAH_PROFILE_RGB_V1_AUTOCORRECT_FEEDBACK_SIZE);
    offset += NOAH_PROFILE_RGB_V1_AUTOCORRECT_FEEDBACK_SIZE;
    for (uint8_t row = 0u; row < row_count; row++) {
        uint8_t group = 0u;

        while (masks[group] != rows[row].led_mask) group++;
        memcpy(&payload[offset], (const uint8_t[]){rows[row].selector, rows[row].color.h, rows[row].color.s, rows[row].color.v, group}, NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE);
        offset += NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE;
    }
    CHECK(offset <= LIVE_PAYLOAD_MAX);
    return offset;
}

static noah_active_profile_rgb_runtime_t live_runtime;
static uint8_t                           live_bytes[LIVE_PAYLOAD_MAX];
static uint32_t                          live_generation;

// Publishes the payload as the active profile's lighting and captures it.
static noah_active_profile_rgb_frame_t live_frame(size_t length) {
    noah_profile_rgb_v1_limits_t    limits   = noah_profile_rgb_v1_default_limits();
    noah_active_profile_snapshot_t  snapshot = {0};
    noah_active_profile_rgb_frame_t frame;
    noah_profile_rgb_v1_error_t     error;

    limits.compiled_stage_mask          = NOAH_PROFILE_RGB_V1_STAGE_LAYER | NOAH_PROFILE_RGB_V1_STAGE_AUTOCORRECT;
    limits.supported_pointing_mode_mask = 0u;
    limits.logical_layer_count          = LAYER_COUNT;
    live_generation++;
    snapshot.identity            = (noah_active_profile_identity_t){.generation = live_generation, .payload_crc32 = live_generation, .payload_digest = live_generation, .compiled_default_digest = 1u, .action_abi_digest = 1u, .kind = NOAH_ACTIVE_PROFILE_KIND_VALIDATED_PROFILE};
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    CHECK(noah_profile_rgb_v1_decode(live_bytes, length, &limits, &snapshot.profile.rgb, &error) == NOAH_PROFILE_RGB_V1_OK);
    noah_active_profile_rgb_runtime_invalidate(&live_runtime, live_generation, (noah_active_profile_identity_t){0}, snapshot.identity, &snapshot);
    CHECK(rgb_active_profile_config_capture_frame(&frame) == NOAH_ACTIVE_PROFILE_RGB_OK);
    return frame;
}

static noah_active_profile_rgb_frame_t live_profile(uint16_t stage_mask, uint8_t locality, const live_row_t *rows, uint8_t row_count) {
    return live_frame(live_payload(live_bytes, stage_mask, (hsv_t){31u, 32u, 33u}, (hsv_t){41u, 42u, 43u}, (hsv_t){51u, 52u, 53u}, (hsv_t){101u, 102u, 103u}, locality, rows, row_count));
}

enum { STAGES_ON = NOAH_PROFILE_RGB_V1_STAGE_LAYER | NOAH_PROFILE_RGB_V1_STAGE_AUTOCORRECT };

static bool render(rgb_runtime_frame_t *frame, const noah_active_profile_rgb_frame_t *profile, noah_autocorrect_feedback_t feedback) {
    return rgb_runtime_autocorrect_feedback_stage_render_active_profile_frame(frame, profile, &feedback, 0u, RGB_MATRIX_LED_COUNT);
}

// ── Tests ───────────────────────────────────────────────────────────────────

// Without a live profile the stage draws the compiled lighting: each semantic
// in its own colour, an inheriting accent in that colour, and a colour of
// saturation and value 0 painting nothing while a coloured accent of its
// semantic still paints.
static void test_the_compiled_lighting_paints_each_semantic(void) {
    rgb_runtime_frame_t frame;

    CHECK(render(&frame, NULL, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_TEXT, 1u << 1u)));
    check_frame(&frame, (1u << 1u) | (1u << 7u), (rgb_t){.r = 11u, .g = 12u, .b = 13u});
    CHECK(render(&frame, NULL, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_MACRO, 1u << 2u)));
    check_frame(&frame, (1u << 2u) | (1u << 7u), (rgb_t){.r = 21u, .g = 22u, .b = 23u});
    CHECK(render(&frame, NULL, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_UNDO, 1u << 2u)));
    check_frame(&frame, 1u << 6u, (rgb_t){.r = 201u, .g = 202u, .b = 203u});
    // Leading paints on the Leader key that began it, and a Leader no key
    // names as a flash whose key is not known: every key.
    CHECK(render(&frame, NULL, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_LEADING, 1u << 3u)));
    check_frame(&frame, (1u << 3u) | (1u << 7u), (rgb_t){.r = 91u, .g = 92u, .b = 93u});
    CHECK(render(&frame, NULL, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_LEADING, 0u)));
    check_frame(&frame, 0xffu, (rgb_t){.r = 91u, .g = 92u, .b = 93u});
}

// Nothing shows without a flash, and a kind the build does not know, as the
// other half could send, paints nothing.
static void test_no_flash_paints_nothing(void) {
    rgb_runtime_frame_t         frame;
    noah_autocorrect_feedback_t unknown = feedback_on(NOAH_AUTOCORRECT_FEEDBACK_TEXT, 1u);

    memset(&frame, 0xff, sizeof(frame));
    CHECK(!render(&frame, NULL, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_NONE, 1u)));
    check_frame(&frame, 0u, (rgb_t){0});
    unknown.kind = (uint8_t)(NOAH_AUTOCORRECT_FEEDBACK_LEADING + 1u);
    CHECK(!render(&frame, NULL, unknown));
    check_frame(&frame, 0u, (rgb_t){0});
    CHECK(!rgb_runtime_autocorrect_feedback_stage_render_active_profile_frame(&frame, NULL, NULL, 0u, RGB_MATRIX_LED_COUNT));
    check_frame(&frame, 0u, (rgb_t){0});
}

// Each locality, for a key on the right half, a combo across both halves and
// a key the typing stage does not know.
static void test_each_locality(void) {
    static const struct {
        uint8_t locality, right_key, across, unknown;
    } cases[] = {
        {RGB_BOTH_HALVES, 0xffu, 0xffu, 0xffu}, {RGB_LEFT_HALF, 0x0fu, 0x0fu, 0x0fu}, {RGB_RIGHT_HALF, 0xf0u, 0xf0u, 0xf0u}, {RGB_KEY_HALF, 0xf0u, 0xffu, 0xffu}, {RGB_KEYS_ONLY, 1u << 6u, (1u << 6u) | (1u << 1u), 0xffu},
    };

    for (uint8_t index = 0u; index < ARRAY_SIZE(cases); index++) {
        noah_active_profile_rgb_frame_t profile = live_profile(STAGES_ON, cases[index].locality, NULL, 0u);
        rgb_runtime_frame_t             frame;

        CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_TEXT, 1u << 6u)));
        check_frame(&frame, cases[index].right_key, text_rgb);
        CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_MACRO, (1u << 6u) | (1u << 1u))));
        check_frame(&frame, cases[index].across, macro_rgb);
        CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_UNDO, 0u)));
        check_frame(&frame, cases[index].unknown, undo_rgb);
        // Leading paints on the Leader key as a flash does on its key, and a
        // Leader no key names as a flash whose key is not known.
        CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_LEADING, 1u << 6u)));
        check_frame(&frame, cases[index].right_key, lead_rgb);
        CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_LEADING, 0u)));
        check_frame(&frame, cases[index].unknown, lead_rgb);
    }
    // A key on the left half lights the left half.
    {
        noah_active_profile_rgb_frame_t profile = live_profile(STAGES_ON, RGB_KEY_HALF, NULL, 0u);
        rgb_runtime_frame_t             frame;

        CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_TEXT, 1u << 2u)));
        check_frame(&frame, 0x0fu, text_rgb);
    }
}

// Accents paint over the flash: every all-semantics row first, then the rows
// naming the semantic that shows, whatever their order in the table; rows of
// another semantic do not paint and are not read.
static void test_accents_follow_their_semantic(void) {
    static const live_row_t rows[] = {
        {.selector = NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_MACRO, .color = {61u, 62u, 63u}, .led_mask = 1u << 5u},
        {.selector = NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_TEXT, .color = {71u, 72u, 73u}, .led_mask = 1u << 5u},
        {.selector = NOAH_PROFILE_RGB_V1_SELECTOR_ALL, .color = {81u, 82u, 83u}, .led_mask = (1u << 5u) | (1u << 4u)},
        {.selector = NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_UNDO, .color = {0u, 0u, 0u}, .led_mask = 1u << 3u},
        {.selector = NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_LEADING, .color = {111u, 112u, 113u}, .led_mask = 1u << 2u},
    };
    noah_active_profile_rgb_frame_t profile = live_profile(STAGES_ON, RGB_KEYS_ONLY, rows, ARRAY_SIZE(rows));
    rgb_runtime_frame_t             frame;

    rgb_active_profile_config_test_reset_reads();
    CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_TEXT, 1u << 0u)));
    CHECK(frame.painted[0] && frame.colors[0].r == text_rgb.r);
    CHECK(frame.painted[4] && frame.colors[4].r == 81u);
    CHECK(frame.painted[5] && frame.colors[5].r == 71u);
    CHECK(!frame.painted[3] && !frame.painted[1] && !frame.painted[6] && !frame.painted[2]);
    CHECK(rgb_active_profile_config_test_reads().autocorrect_rows == 2u);

    CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_MACRO, 1u << 0u)));
    CHECK(frame.painted[5] && frame.colors[5].r == 61u && frame.painted[4] && frame.colors[4].r == 81u && !frame.painted[3]);
    // A black row takes the colour of the semantic that shows.
    CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_UNDO, 1u << 0u)));
    CHECK(frame.painted[3] && frame.colors[3].r == undo_rgb.r && frame.colors[3].b == undo_rgb.b);
    CHECK(frame.painted[5] && frame.colors[5].r == 81u);
    // Leading's own accent over its colour on every key.
    CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_LEADING, 0u)));
    CHECK(frame.painted[0] && frame.colors[0].r == lead_rgb.r && frame.painted[2] && frame.colors[2].r == 111u && frame.painted[4] && frame.colors[4].r == 81u);
}

// A semantic whose colour has saturation and value 0, at any hue, paints
// nothing by itself or through a black accent; the other semantics still do.
static void test_a_colour_that_is_off_paints_nothing(void) {
    static const live_row_t         rows[]  = {{.selector = NOAH_PROFILE_RGB_V1_SELECTOR_ALL, .color = {0u, 0u, 0u}, .led_mask = 1u << 7u}};
    noah_active_profile_rgb_frame_t profile = live_frame(live_payload(live_bytes, STAGES_ON, (hsv_t){31u, 32u, 33u}, (hsv_t){90u, 0u, 0u}, (hsv_t){51u, 52u, 53u}, (hsv_t){7u, 0u, 0u}, RGB_BOTH_HALVES, rows, ARRAY_SIZE(rows)));
    rgb_runtime_frame_t             frame;

    CHECK(!render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_MACRO, 1u)));
    check_frame(&frame, 0u, (rgb_t){0});
    CHECK(!render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_LEADING, 0u)));
    check_frame(&frame, 0u, (rgb_t){0});
    CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_TEXT, 1u)));
    check_frame(&frame, 0xffu, text_rgb);
}

// With leading's colour off, a coloured accent for it still paints: leading
// can show on a few LEDs alone (D-F72).
static void test_leading_can_show_on_its_accent_alone(void) {
    static const live_row_t         rows[]  = {{.selector = NOAH_PROFILE_RGB_V1_AUTOCORRECT_SEMANTIC_LEADING, .color = {121u, 122u, 123u}, .led_mask = 1u << 6u}};
    noah_active_profile_rgb_frame_t profile = live_frame(live_payload(live_bytes, STAGES_ON, (hsv_t){31u, 32u, 33u}, (hsv_t){41u, 42u, 43u}, (hsv_t){51u, 52u, 53u}, (hsv_t){7u, 0u, 0u}, RGB_BOTH_HALVES, rows, ARRAY_SIZE(rows)));
    rgb_runtime_frame_t             frame;

    CHECK(render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_LEADING, 0u)));
    check_frame(&frame, 1u << 6u, (rgb_t){.r = 121u, .g = 122u, .b = 123u});
}

// A stage switched off keeps its data and draws nothing.
static void test_the_stage_bit_switches_it_off(void) {
    static const live_row_t         rows[]  = {{.selector = NOAH_PROFILE_RGB_V1_SELECTOR_ALL, .color = {81u, 82u, 83u}, .led_mask = 1u << 7u}};
    noah_active_profile_rgb_frame_t profile = live_profile(NOAH_PROFILE_RGB_V1_STAGE_LAYER, RGB_BOTH_HALVES, rows, ARRAY_SIZE(rows));
    rgb_runtime_frame_t             frame;

    CHECK(!render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_TEXT, 1u)));
    check_frame(&frame, 0u, (rgb_t){0});
    CHECK(!render(&frame, &profile, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_LEADING, 0u)));
    check_frame(&frame, 0u, (rgb_t){0});
}

// A chunk paints only its own LEDs and leaves the rest of the frame alone.
static void test_a_chunk_paints_only_its_leds(void) {
    noah_active_profile_rgb_frame_t profile  = live_profile(STAGES_ON, RGB_BOTH_HALVES, NULL, 0u);
    noah_autocorrect_feedback_t     feedback = feedback_on(NOAH_AUTOCORRECT_FEEDBACK_TEXT, 1u);
    rgb_runtime_frame_t             frame    = {0};

    CHECK(rgb_runtime_autocorrect_feedback_stage_render_active_profile_frame(&frame, &profile, &feedback, 2u, 6u));
    check_frame(&frame, 0x3cu, text_rgb);
}

// A profile published after the frame was captured shows nothing of the old one.
static void test_a_stale_frame_paints_nothing(void) {
    noah_active_profile_rgb_frame_t stale = live_profile(STAGES_ON, RGB_BOTH_HALVES, NULL, 0u);
    rgb_runtime_frame_t             frame;

    (void)live_profile(STAGES_ON, RGB_LEFT_HALF, NULL, 0u);
    CHECK(!render(&frame, &stale, feedback_on(NOAH_AUTOCORRECT_FEEDBACK_TEXT, 1u)));
    check_frame(&frame, 0u, (rgb_t){0});
}

int main(void) {
    test_the_compiled_lighting_paints_each_semantic();
    test_no_flash_paints_nothing();
    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    test_each_locality();
    test_accents_follow_their_semantic();
    test_a_colour_that_is_off_paints_nothing();
    test_leading_can_show_on_its_accent_alone();
    test_the_stage_bit_switches_it_off();
    test_a_chunk_paints_only_its_leds();
    test_a_stale_frame_paints_nothing();
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
    puts("rgb active-profile replacement-feedback host tests passed");
    return 0;
}
