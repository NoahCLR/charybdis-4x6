#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/rgb/automouse/rgb_automouse.h"
#include "users/noah/lib/rgb/automouse/rgb_automouse_stage.h"
#include "users/noah/lib/rgb/core/rgb_config_helpers.h"
#include "users/noah/lib/rgb/stages/rgb_layer_stage.h"
#include "ws2812.h"

enum test_layers {
    LAYER_BASE = 0,
    LAYER_NAV,
    LAYER_SYM,
};

enum {
    TEST_RGB_PAYLOAD_SIZE = NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE,
};

static uint16_t test_keymap[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
static rgb_t    led_output[RGB_MATRIX_LED_COUNT];
static uint16_t fake_automouse_progress;

layer_state_t layer_state  = 0;
led_config_t  g_led_config = {0};
ws2812_led_t  ws2812_leds[WS2812_LED_COUNT];

const layer_color_config_t layer_colors[LAYER_COUNT] = {
    [LAYER_BASE] =
        {
            .color = {.h = 101, .s = 102, .v = 103},
            .mode  = ALL_KEYS,
        },
    [LAYER_NAV] =
        {
            .color = {.h = 110, .s = 120, .v = 130},
            .mode  = ALL_KEYS,
        },
    [LAYER_SYM] =
        {
            .color = {.h = 140, .s = 150, .v = 160},
            .mode  = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
        },
};

static const layer_led_group_t layer_led_groups_data[] = {
    {
        .layer     = LAYER_BASE,
        .color     = {.h = 7, .s = 8, .v = 9},
        .led_group = RGB_LED_GROUP(3),
    },
    {
        .layer     = RGB_LAYER_GROUP_ALL,
        .color     = HSV(0, 0, 0),
        .led_group = RGB_LED_GROUP(2),
    },
};

const layer_led_group_t *const layer_led_groups      = layer_led_groups_data;
const uint8_t                  layer_led_group_count = (uint8_t)ARRAY_SIZE(layer_led_groups_data);

const automouse_fade_end_config_t automouse_fade_end_config = {
    .mode      = FOLLOW_REAL_DESTINATION,
    .end_color = HSV(200, 201, 202),
};

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

static rgb_t rgb_from_hsv(hsv_t hsv) {
    return (rgb_t){.r = hsv.h, .g = hsv.s, .b = hsv.v};
}

typedef struct {
    const uint8_t                    *bytes;
    noah_effective_rgb_runtime_t     *runtime;
    noah_effective_profile_identity_t previous;
    noah_effective_profile_identity_t active;
    noah_effective_profile_snapshot_t *snapshot;
    size_t                            calls;
    size_t                            invalidate_on_call;
} invalidating_reader_t;

static bool invalidating_reader_read(void *context_value, size_t offset, uint8_t *target, size_t length) {
    invalidating_reader_t *context = context_value;

    memcpy(target, &context->bytes[offset], length);
    context->calls++;
    if (context->calls == context->invalidate_on_call) {
        noah_effective_rgb_runtime_invalidate(context->runtime, 2u, context->previous, context->active, context->snapshot);
    }
    return true;
}

static bool fixture_value(const char *path, const char *key, char *value, size_t capacity) {
    FILE  *file = fopen(path, "r");
    char   line[TEST_RGB_PAYLOAD_SIZE * 2u + 64u];
    size_t key_length = strlen(key);

    CHECK(file != NULL);
    while (fgets(line, sizeof(line), file)) {
        size_t length;
        if (strncmp(line, key, key_length) != 0 || line[key_length] != '=') continue;
        length = strcspn(&line[key_length + 1u], "\r\n");
        CHECK(length + 1u <= capacity);
        memcpy(value, &line[key_length + 1u], length);
        value[length] = '\0';
        fclose(file);
        return true;
    }
    fclose(file);
    return false;
}

static uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if (value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    if (value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
    test_fail("hex nibble", __FILE__, __LINE__);
    return 0u;
}

static size_t fixture_payload(const char *path, uint8_t *payload, size_t capacity) {
    char   encoded[TEST_RGB_PAYLOAD_SIZE * 2u + 1u];
    size_t length;

    CHECK(fixture_value(path, "payload.hex", encoded, sizeof(encoded)));
    length = strlen(encoded);
    CHECK((length % 2u) == 0u && length / 2u <= capacity);
    for (size_t index = 0u; index < length; index += 2u) {
        payload[index / 2u] = (uint8_t)((hex_nibble(encoded[index]) << 4u) | hex_nibble(encoded[index + 1u]));
    }
    return length / 2u;
}

static void check_frame_led(const rgb_runtime_frame_t *frame, uint8_t index, rgb_t expected) {
    CHECK(frame->painted[index]);
    CHECK(frame->colors[index].r == expected.r);
    CHECK(frame->colors[index].g == expected.g);
    CHECK(frame->colors[index].b == expected.b);
}

static void check_output_led(uint8_t index, rgb_t expected) {
    CHECK(led_output[index].r == expected.r);
    CHECK(led_output[index].g == expected.g);
    CHECK(led_output[index].b == expected.b);
}

static void test_reset(void) {
    memset(test_keymap, 0, sizeof(test_keymap));
    memset(&g_led_config, 0xFF, sizeof(g_led_config));
    memset(led_output, 0, sizeof(led_output));
    memset(ws2812_leds, 0, sizeof(ws2812_leds));
    layer_state             = 0;
    fake_automouse_progress = 0u;

    g_led_config.matrix_co[0][0] = 0;
    g_led_config.matrix_co[0][1] = 1;
    g_led_config.matrix_co[0][2] = 2;
    g_led_config.matrix_co[0][3] = 3;

    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                test_keymap[layer][row][col] = KC_TRNS;
            }
        }
    }

    rgb_runtime_layer_stage_post_init();
    rgb_runtime_automouse_stage_post_init();
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return test_keymap[layer_num][row][column];
}

rgb_t hsv_to_rgb(hsv_t hsv) {
    return rgb_from_hsv(hsv);
}

void rgb_matrix_set_color(int index, uint8_t red, uint8_t green, uint8_t blue) {
    CHECK(index >= 0 && index < RGB_MATRIX_LED_COUNT);
    led_output[index] = (rgb_t){.r = red, .g = green, .b = blue};
}

int rgb_matrix_led_index(int index) {
    return index;
}

uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t column, uint8_t *led_i) {
    uint8_t led = g_led_config.matrix_co[row][column];
    if (led == NO_LED) {
        return 0;
    }

    led_i[0] = led;
    return 1;
}

uint8_t get_auto_mouse_layer(void) {
    return LAYER_SYM;
}

uint16_t automouse_rgb_current_progress(void) {
    return fake_automouse_progress;
}

bool automouse_rgb_should_render(void) {
    return fake_automouse_progress != 0u;
}

static void test_base_layer_is_visible_when_no_overlay_layer_is_active(void) {
    test_reset();

    rgb_runtime_frame_t frame;

    CHECK(rgb_runtime_layer_stage_render_frame(&frame, 0, 0, RGB_MATRIX_LED_COUNT));

    check_frame_led(&frame, 0, rgb_from_hsv(layer_colors[LAYER_BASE].color));
    check_frame_led(&frame, 1, rgb_from_hsv(layer_colors[LAYER_BASE].color));
    check_frame_led(&frame, 2, rgb_from_hsv(layer_colors[LAYER_BASE].color));
    check_frame_led(&frame, 3, rgb_from_hsv(layer_led_groups[0].color));
}

static void test_base_layer_remains_under_mapped_only_overlay_layers(void) {
    test_reset();

    test_keymap[LAYER_SYM][0][1] = 0x004Fu;
    layer_state                  = (layer_state_t)1u << LAYER_SYM;

    rgb_runtime_frame_t frame;

    CHECK(rgb_runtime_layer_stage_render_frame(&frame, layer_state, 0, RGB_MATRIX_LED_COUNT));

    check_frame_led(&frame, 0, rgb_from_hsv(layer_colors[LAYER_BASE].color));
    check_frame_led(&frame, 1, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_frame_led(&frame, 2, rgb_from_hsv(layer_colors[LAYER_SYM].color));
    check_frame_led(&frame, 3, rgb_from_hsv(layer_led_groups[0].color));
}

static noah_effective_profile_identity_t test_identity(uint32_t generation) {
    return (noah_effective_profile_identity_t){
        .generation              = generation,
        .payload_crc32           = 0x1000u + generation,
        .payload_digest          = 0x2000u + generation,
        .compiled_default_digest = 0x3000u,
        .action_abi_digest       = 0x4000u,
        .origin                  = 0u,
        .kind                    = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE,
    };
}

static void test_live_layer_profile_and_stale_frame(const char *fixture_path) {
    uint8_t                           payload[TEST_RGB_PAYLOAD_SIZE];
    size_t                            length = fixture_payload(fixture_path, payload, sizeof(payload));
    noah_profile_rgb_v1_limits_t      limits = noah_profile_rgb_v1_default_limits();
    noah_profile_rgb_v1_view_t        view;
    noah_profile_rgb_v1_error_t       error;
    noah_effective_rgb_runtime_t      runtime;
    noah_effective_rgb_frame_t        profile_frame;
    noah_effective_profile_identity_t active = test_identity(1u);
    noah_effective_profile_snapshot_t snapshot = {0};
    rgb_runtime_frame_t               frame;

    limits.logical_layer_count    = LAYER_COUNT;
    limits.maximum_brightness     = 200u;
    limits.tap_branch_color_count = 4u;
    CHECK(noah_profile_rgb_v1_decode(payload, length, &limits, &view, &error) == NOAH_PROFILE_RGB_V1_OK);
    snapshot.identity            = active;
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    snapshot.profile.rgb         = view;

    noah_effective_rgb_runtime_init(&runtime);
    CHECK(noah_effective_rgb_runtime_install(&runtime));
    noah_effective_rgb_runtime_invalidate(&runtime, 1u, (noah_effective_profile_identity_t){0}, active, &snapshot);
    CHECK(rgb_effective_config_capture_frame(&profile_frame) == NOAH_EFFECTIVE_RGB_OK);

    test_reset();
    CHECK(rgb_runtime_layer_stage_render_effective_frame(&frame, 0u, &profile_frame, 0u, RGB_MATRIX_LED_COUNT));
    check_frame_led(&frame, 0u, rgb_from_hsv((hsv_t){.h = 1u, .s = 2u, .v = 3u}));
    check_frame_led(&frame, 1u, rgb_from_hsv((hsv_t){.h = 1u, .s = 2u, .v = 3u}));
    check_frame_led(&frame, 2u, rgb_from_hsv((hsv_t){.h = 1u, .s = 2u, .v = 3u}));
    check_frame_led(&frame, 3u, rgb_from_hsv((hsv_t){.h = 1u, .s = 2u, .v = 3u}));

    test_keymap[LAYER_NAV][0][1] = 0x004fu;
    layer_state                  = (layer_state_t)1u << LAYER_NAV;
    CHECK(rgb_runtime_layer_stage_render_effective_frame(&frame, layer_state, &profile_frame, 0u, RGB_MATRIX_LED_COUNT));
    check_frame_led(&frame, 0u, rgb_from_hsv((hsv_t){.h = 10u, .s = 20u, .v = 30u}));
    check_frame_led(&frame, 1u, rgb_from_hsv((hsv_t){.h = 10u, .s = 20u, .v = 30u}));
    check_frame_led(&frame, 2u, rgb_from_hsv((hsv_t){.h = 1u, .s = 2u, .v = 3u}));
    check_frame_led(&frame, 3u, rgb_from_hsv((hsv_t){.h = 1u, .s = 2u, .v = 3u}));

    noah_effective_profile_identity_t next = test_identity(2u);
    noah_effective_profile_snapshot_t behavior_only = {.identity = next};
    invalidating_reader_t             reader = {
        .bytes              = payload,
        .runtime            = &runtime,
        .previous           = active,
        .active             = next,
        .snapshot           = &behavior_only,
        .invalidate_on_call = 4u,
    };
    profile_frame.view.reader = (noah_profile_reader_t){
        .read    = invalidating_reader_read,
        .context = &reader,
        .length  = length,
    };

    CHECK(!rgb_runtime_layer_stage_render_effective_frame(&frame, layer_state, &profile_frame, 0u, RGB_MATRIX_LED_COUNT));
    CHECK(reader.calls > reader.invalidate_on_call);
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        CHECK(!frame.painted[led]);
    }
    noah_effective_rgb_runtime_uninstall(&runtime);
}

static void test_live_automouse_profile_and_stale_frame(const char *fixture_path) {
    uint8_t                           payload[TEST_RGB_PAYLOAD_SIZE];
    size_t                            length = fixture_payload(fixture_path, payload, sizeof(payload));
    noah_profile_rgb_v1_limits_t      limits = noah_profile_rgb_v1_default_limits();
    noah_profile_rgb_v1_view_t        view;
    noah_profile_rgb_v1_error_t       error;
    noah_effective_rgb_runtime_t      runtime;
    noah_effective_rgb_frame_t        profile_frame;
    noah_effective_profile_identity_t active = test_identity(10u);
    noah_effective_profile_snapshot_t snapshot = {0};
    rgb_t                             compiled_base = rgb_from_hsv(layer_colors[LAYER_BASE].color);
    rgb_t                             live_end      = rgb_from_hsv((hsv_t){.h = 8u, .s = 9u, .v = 10u});

    test_reset();
    layer_state             = (layer_state_t)1u << LAYER_SYM;
    fake_automouse_progress = AUTOMOUSE_RGB_ACTIVE_SPAN;
    CHECK(rgb_runtime_automouse_stage_should_render(layer_state));
    CHECK(rgb_runtime_automouse_stage_render(layer_state, 0u, RGB_MATRIX_LED_COUNT));
    check_output_led(0u, compiled_base);
    check_output_led(1u, compiled_base);
    check_output_led(2u, compiled_base);
    check_output_led(3u, rgb_from_hsv(layer_led_groups[0].color));

    limits.logical_layer_count    = LAYER_COUNT;
    limits.maximum_brightness     = 200u;
    limits.tap_branch_color_count = 4u;
    CHECK(noah_profile_rgb_v1_decode(payload, length, &limits, &view, &error) == NOAH_PROFILE_RGB_V1_OK);
    snapshot.identity            = active;
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    snapshot.profile.rgb         = view;

    noah_effective_rgb_runtime_init(&runtime);
    CHECK(noah_effective_rgb_runtime_install(&runtime));
    noah_effective_rgb_runtime_invalidate(&runtime, 1u, (noah_effective_profile_identity_t){0}, active, &snapshot);
    CHECK(rgb_effective_config_capture_frame(&profile_frame) == NOAH_EFFECTIVE_RGB_OK);

    test_reset();
    layer_state             = (layer_state_t)1u << LAYER_SYM;
    fake_automouse_progress = AUTOMOUSE_RGB_ACTIVE_SPAN;
    CHECK(rgb_runtime_automouse_stage_should_render_effective(layer_state, &profile_frame));
    CHECK(rgb_runtime_automouse_stage_render_effective(layer_state, &profile_frame, 0u, RGB_MATRIX_LED_COUNT));
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        check_output_led(led, live_end);
    }

    noah_effective_rgb_frame_t disabled_frame = profile_frame;
    disabled_frame.view.stage_enable_mask &= (uint16_t)~NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE;
    CHECK(!rgb_runtime_automouse_stage_should_render_effective(layer_state, &disabled_frame));

    noah_effective_profile_identity_t next          = test_identity(11u);
    noah_effective_profile_snapshot_t behavior_only = {.identity = next};
    invalidating_reader_t             reader        = {
        .bytes              = payload,
        .runtime            = &runtime,
        .previous           = active,
        .active             = next,
        .snapshot           = &behavior_only,
        .invalidate_on_call = SIZE_MAX,
    };
    profile_frame.view.reader = (noah_profile_reader_t){.read = invalidating_reader_read, .context = &reader, .length = length};
    CHECK(rgb_runtime_automouse_stage_render_effective(layer_state, &profile_frame, 0u, RGB_MATRIX_LED_COUNT));
    CHECK(reader.calls > 1u);

    size_t final_reader_call = reader.calls;
    rgb_t  sentinel          = {.r = 231u, .g = 232u, .b = 233u};
    reader.calls              = 0u;
    reader.invalidate_on_call = final_reader_call;
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        led_output[led] = sentinel;
    }

    CHECK(!rgb_runtime_automouse_stage_render_effective(layer_state, &profile_frame, 0u, RGB_MATRIX_LED_COUNT));
    CHECK(reader.calls == final_reader_call);
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        check_output_led(led, sentinel);
    }
    noah_effective_rgb_runtime_uninstall(&runtime);
}

static void test_live_automouse_policy_modes(const char *fixture_path) {
    uint8_t payload[TEST_RGB_PAYLOAD_SIZE];
    size_t length = fixture_payload(fixture_path, payload, sizeof(payload));
    size_t layer_offset = 16u + (size_t)payload[4] * 9u;
    size_t fade_offset = layer_offset + (size_t)payload[5] * 5u + (size_t)payload[6] * 5u;
    // A pass-through base and mapped-only lower layer distinguish all policies.
    payload[layer_offset + 1] = payload[layer_offset + 2] = payload[layer_offset + 3] = 0;
    noah_profile_rgb_v1_limits_t limits = noah_profile_rgb_v1_default_limits();
    limits.logical_layer_count = LAYER_COUNT; limits.maximum_brightness = 200; limits.tap_branch_color_count = 4;
    noah_effective_rgb_runtime_t runtime;
    noah_effective_rgb_runtime_init(&runtime);
    CHECK(noah_effective_rgb_runtime_install(&runtime));
    for (uint8_t mode = 0; mode < 3; mode++) {
        payload[fade_offset] = mode;
        noah_effective_profile_snapshot_t snapshot = {.identity = test_identity(20u + mode)};
        noah_profile_rgb_v1_error_t error;
        CHECK(noah_profile_rgb_v1_decode(payload, length, &limits, &snapshot.profile.rgb, &error) == NOAH_PROFILE_RGB_V1_OK);
        snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
        noah_effective_rgb_runtime_invalidate(&runtime, mode + 1, (noah_effective_profile_identity_t){0}, snapshot.identity, &snapshot);
        noah_effective_rgb_frame_t frame;
        CHECK(rgb_effective_config_capture_frame(&frame) == NOAH_EFFECTIVE_RGB_OK);
        test_reset();
        test_keymap[LAYER_NAV][0][1] = 4;
        layer_state = ((layer_state_t)1u << LAYER_NAV) | ((layer_state_t)1u << LAYER_SYM);
        fake_automouse_progress = AUTOMOUSE_RGB_ACTIVE_SPAN;
        rgb_runtime_frame_t destination;
        CHECK(rgb_runtime_layer_stage_render_effective_frame(&destination, (layer_state_t)1u << LAYER_NAV, &frame, 0, RGB_MATRIX_LED_COUNT));
        for (uint8_t led = 0; led < RGB_MATRIX_LED_COUNT; led++) ws2812_leds[led] = (ws2812_led_t){.r = 21, .g = 22, .b = 23};
        CHECK(rgb_runtime_automouse_stage_render_effective(layer_state, &frame, 0, RGB_MATRIX_LED_COUNT));
        unsigned retained = 0, filled = 0;
        for (uint8_t led = 0; led < RGB_MATRIX_LED_COUNT; led++) {
            rgb_t expected;
            if (mode == 2 || (mode == 1 && !destination.painted[led])) expected = rgb_from_hsv((hsv_t){.h = 8, .s = 9, .v = 10});
            else if (destination.painted[led]) expected = destination.colors[led];
            else expected = (rgb_t){.r = 21, .g = 22, .b = 23};
            check_output_led(led, expected);
            if (destination.painted[led]) retained++; else filled++;
        }
        CHECK(retained > 0 && filled > 0);
    }
    noah_effective_rgb_runtime_uninstall(&runtime);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    test_base_layer_is_visible_when_no_overlay_layer_is_active();
    test_base_layer_remains_under_mapped_only_overlay_layers();
    test_live_layer_profile_and_stale_frame(argv[1]);
    test_live_automouse_profile_and_stale_frame(argv[1]);
    test_live_automouse_policy_modes(argv[1]);

    puts("rgb_base_underlay host tests passed");
    return 0;
}
