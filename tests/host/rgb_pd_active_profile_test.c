#include QMK_KEYBOARD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/key/runtime/slot/origin_registry.h"
#include "users/noah/lib/rgb/core/rgb_config_helpers.h"
#include "users/noah/lib/rgb/stages/rgb_pointing_mode_stage.h"

enum {
    TEST_RGB_PAYLOAD_SIZE = NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE,
};

const layer_color_config_t     layer_colors[LAYER_COUNT] = {0};
const layer_led_group_t *const layer_led_groups          = NULL;
const uint8_t                  layer_led_group_count     = 0u;

const pointing_mode_color_t pointing_mode_colors[] = {
    {.pointing_mode = POINTING_MODE_DRAGSCROLL, .color = HSV(101, 102, 103), .locality = RGB_LEFT_HALF}, {.pointing_mode = POINTING_MODE_VOLUME, .color = HSV(111, 112, 113), .locality = RGB_LEFT_HALF}, {.pointing_mode = POINTING_MODE_BRIGHTNESS, .color = HSV(121, 122, 123), .locality = RGB_LEFT_HALF}, {.pointing_mode = POINTING_MODE_ZOOM, .color = HSV(131, 132, 133), .locality = RGB_LEFT_HALF}, {.pointing_mode = POINTING_MODE_ARROW, .color = HSV(141, 142, 143), .locality = RGB_LEFT_HALF}, {.pointing_mode = POINTING_MODE_PINCH, .color = HSV(151, 152, 153), .locality = RGB_LEFT_HALF},
    // Current RGB geometry includes an explicit color row for every slot.
    {.pointing_mode = POINTING_MODE_6, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_7, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_8, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_9, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_10, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_11, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_12, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_13, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_14, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_15, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_16, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_17, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_18, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_19, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_20, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_21, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_22, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_23, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_24, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_25, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_26, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_27, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_28, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_29, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_30, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},
    {.pointing_mode = POINTING_MODE_31, .color = HSV(0, 0, 0), .locality = RGB_BOTH_HALVES},

};
const uint8_t pointing_mode_color_count = (uint8_t)ARRAY_SIZE(pointing_mode_colors);

static const pointing_mode_led_group_t pointing_mode_led_groups_data[] = {
    {.pointing_mode = POINTING_MODE_VOLUME, .color = HSV(201, 202, 203), .led_group = RGB_LED_GROUP(2)},
    {.pointing_mode = RGB_POINTING_MODE_GROUP_ALL, .color = HSV(0, 0, 0), .led_group = RGB_LED_GROUP(3)},
};
const pointing_mode_led_group_t *const pointing_mode_led_groups = pointing_mode_led_groups_data;
const uint8_t                    pointing_mode_led_group_count = (uint8_t)ARRAY_SIZE(pointing_mode_led_groups_data);

static pointing_mode_snapshot_t fake_snapshot;

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

pointing_mode_snapshot_t pointing_mode_snapshot_with_owner_bitmap(uint8_t *out_owner_bitmap, bool *out_has_owner_keys) {
    if (out_owner_bitmap) {
        memset(out_owner_bitmap, 0, KEY_ORIGIN_BITMAP_SIZE);
    }
    if (out_has_owner_keys) {
        *out_has_owner_keys = false;
    }
    return fake_snapshot;
}

pointing_mode_snapshot_t pointing_mode_snapshot(void) {
    return fake_snapshot;
}

// The running mode's link; none unless a test sets one, so a running mode
// paints in its own stage.
static uint16_t fake_link_layers;

uint16_t pointing_mode_link_layers(pointing_mode_mask_t mode) {
    return mode != 0 ? fake_link_layers : 0u;
}

uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t column, uint8_t *led_i) {
    if (!(led_i && row < MATRIX_ROWS && column < MATRIX_COLS)) {
        return 0u;
    }
    led_i[0] = (uint8_t)(row * MATRIX_COLS + column);
    return 1u;
}

rgb_t hsv_to_rgb(hsv_t hsv) {
    return (rgb_t){.r = hsv.h, .g = hsv.s, .b = hsv.v};
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

static noah_active_profile_identity_t test_identity(uint32_t generation) {
    return (noah_active_profile_identity_t){
        .generation              = generation,
        .payload_crc32           = 0x1000u + generation,
        .payload_digest          = 0x2000u + generation,
        .compiled_default_digest = 0x3000u,
        .action_abi_digest       = 0x4000u,
        .kind                    = NOAH_ACTIVE_PROFILE_KIND_VALIDATED_PROFILE,
    };
}

static void check_led(const rgb_runtime_frame_t *frame, uint8_t led, rgb_t expected) {
    CHECK(frame->painted[led]);
    CHECK(frame->colors[led].r == expected.r);
    CHECK(frame->colors[led].g == expected.g);
    CHECK(frame->colors[led].b == expected.b);
}

static void check_unpainted(const rgb_runtime_frame_t *frame, uint8_t led) {
    CHECK(!frame->painted[led]);
}

typedef struct {
    const uint8_t                     *bytes;
    noah_active_profile_rgb_runtime_t *runtime;
    noah_active_profile_identity_t     previous;
    noah_active_profile_identity_t     active;
    noah_active_profile_snapshot_t *snapshot;
    size_t                             calls;
    size_t                             invalidate_on_call;
} invalidating_reader_t;

static bool invalidating_reader_read(void *context_value, size_t offset, uint8_t *target, size_t length) {
    invalidating_reader_t *context = context_value;

    memcpy(target, &context->bytes[offset], length);
    context->calls++;
    if (context->calls == context->invalidate_on_call) {
        noah_active_profile_rgb_runtime_invalidate(context->runtime, 2u, context->previous, context->active, context->snapshot);
    }
    return true;
}

static void test_compiled_and_live_pd_rendering(const char *fixture_path) {
    uint8_t                           payload[TEST_RGB_PAYLOAD_SIZE];
    size_t                            length = fixture_payload(fixture_path, payload, sizeof(payload));
    noah_profile_rgb_v1_limits_t      limits = noah_profile_rgb_v1_default_limits();
    noah_profile_rgb_v1_view_t        view;
    noah_profile_rgb_v1_error_t       error;
    noah_active_profile_rgb_runtime_t runtime;
    noah_active_profile_rgb_frame_t   profile_frame;
    noah_active_profile_identity_t active      = test_identity(1u);
    noah_active_profile_snapshot_t snapshot = {0};
    rgb_runtime_frame_t               frame;
    rgb_t                             compiled_color = hsv_to_rgb(pointing_mode_colors[POINTING_MODE_INDEX_VOLUME].color);
    rgb_t                             compiled_group = hsv_to_rgb(pointing_mode_led_groups[0].color);
    rgb_t                             live_color     = hsv_to_rgb((hsv_t){.h = 21u, .s = 22u, .v = 23u});

    memset(&fake_snapshot, 0, sizeof(fake_snapshot));
    fake_snapshot.display.active_index = POINTING_MODE_INDEX_VOLUME;
    fake_snapshot.display.active_mode  = POINTING_MODE_VOLUME;

    CHECK(rgb_runtime_pointing_mode_stage_render_frame(&frame, 0u, RGB_MATRIX_LED_COUNT));
    check_led(&frame, 0u, compiled_color);
    check_led(&frame, 1u, compiled_color);
    check_led(&frame, 2u, compiled_group);
    check_led(&frame, 3u, compiled_color);
    for (uint8_t led = RGB_LEFT_LED_COUNT; led < RGB_MATRIX_LED_COUNT; led++) {
        check_unpainted(&frame, led);
    }

    // This rendering fixture models six supported slots with current RGB v3.
    limits.supported_pointing_mode_mask = 0x3fu;
    limits.logical_layer_count    = LAYER_COUNT;
    limits.maximum_brightness     = 200u;
    limits.tap_branch_color_count = 4u;
    CHECK(noah_profile_rgb_v1_decode(payload, length, &limits, &view, &error) == NOAH_PROFILE_RGB_V1_OK);
    snapshot.identity            = active;
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    snapshot.profile.rgb         = view;

    noah_active_profile_rgb_runtime_init(&runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&runtime));
    noah_active_profile_rgb_runtime_invalidate(&runtime, 1u, (noah_active_profile_identity_t){0}, active, &snapshot);
    CHECK(rgb_active_profile_config_capture_frame(&profile_frame) == NOAH_ACTIVE_PROFILE_RGB_OK);
    CHECK(rgb_runtime_pointing_mode_stage_render_active_profile_frame(&frame, &profile_frame, 0u, RGB_MATRIX_LED_COUNT));
    check_led(&frame, 0u, live_color);
    check_led(&frame, 1u, live_color);
    check_unpainted(&frame, 2u);
    check_unpainted(&frame, 3u);
    for (uint8_t led = RGB_LEFT_LED_COUNT; led < RGB_MATRIX_LED_COUNT; led++) {
        check_led(&frame, led, live_color);
    }

    // A linked mode paints beneath the lowest layer of its link that is on,
    // and there only while that link still names it (D-F62).
    fake_link_layers = (uint16_t)((1u << 1) | (1u << 2));
    CHECK(rgb_runtime_pointing_mode_stage_beneath_layer(0u, &profile_frame) == POINTING_MODE_NO_LAYER);
    CHECK(rgb_runtime_pointing_mode_stage_beneath_layer((layer_state_t)((1u << 1) | (1u << 2)), &profile_frame) == 1u);
    CHECK(rgb_runtime_pointing_mode_stage_beneath_layer((layer_state_t)1u << 2, &profile_frame) == 2u);
    CHECK(rgb_runtime_pointing_mode_stage_render_active_profile_beneath_frame(&frame, &profile_frame, 2u, 0u, RGB_MATRIX_LED_COUNT));
    check_led(&frame, 0u, live_color);
    CHECK(!rgb_runtime_pointing_mode_stage_render_active_profile_beneath_frame(&frame, &profile_frame, 0u, 0u, RGB_MATRIX_LED_COUNT));
    // With its stage off the mode has nothing to paint, so the layer stack
    // stays whole and its rows keep their order.
    noah_active_profile_rgb_frame_t stage_off = profile_frame;
    stage_off.view.stage_enable_mask &= (uint16_t)~NOAH_PROFILE_RGB_V1_STAGE_POINTING_MODE;
    CHECK(rgb_runtime_pointing_mode_stage_beneath_layer((layer_state_t)1u << 2, &stage_off) == POINTING_MODE_NO_LAYER);
    fake_link_layers = 0u;

    noah_active_profile_identity_t next             = test_identity(2u);
    noah_active_profile_snapshot_t behavior_only = {.identity = next};
    invalidating_reader_t             reader        = {
        .bytes              = payload,
        .runtime            = &runtime,
        .previous           = active,
        .active             = next,
        .snapshot           = &behavior_only,
        // The render's last read: the active slot's colour row is now read
        // directly by index (one read, not a scan from row 0).
        .invalidate_on_call = 5u,
    };
    profile_frame.view.reader = (noah_profile_reader_t){.read = invalidating_reader_read, .context = &reader, .length = length};
    CHECK(!rgb_runtime_pointing_mode_stage_render_active_profile_frame(&frame, &profile_frame, 0u, RGB_MATRIX_LED_COUNT));
    CHECK(reader.calls == reader.invalidate_on_call);
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        check_unpainted(&frame, led);
    }
    noah_active_profile_rgb_runtime_uninstall(&runtime);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    test_compiled_and_live_pd_rendering(argv[1]);
    puts("rgb active-profile pointing-mode host tests passed");
    return 0;
}
