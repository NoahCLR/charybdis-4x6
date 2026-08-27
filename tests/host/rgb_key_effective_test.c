#include QMK_KEYBOARD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/key/runtime/feedback.h"
#include "users/noah/lib/rgb/core/rgb_config_helpers.h"
#include "users/noah/lib/rgb/stages/rgb_key_feedback_stage.h"

enum {
    TEST_RGB_PAYLOAD_SIZE = NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE,
};

const layer_color_config_t     layer_colors[LAYER_COUNT] = {0};
const layer_led_group_t *const layer_led_groups          = NULL;
const uint8_t                  layer_led_group_count     = 0u;

const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {
    RGB_TAP_BRANCH_COLORS(HSV(11, 12, 13), HSV(21, 22, 23), HSV(31, 32, 33), HSV(41, 42, 43)), .tap_committed_color = HSV(51, 52, 53), .hold_active_color = HSV(61, 62, 63), .long_hold_active_color = HSV(71, 72, 73), .tap_commit_mode = KEY_FEEDBACK_TAP_COMMIT_OFF, .locality = RGB_KEYS_ONLY,
};

static const key_behavior_feedback_led_group_t key_behavior_feedback_led_groups_data[] = {
    {.semantic = KEY_FEEDBACK_GROUP_ALL, .color = HSV(201, 202, 203), .led_group = RGB_LED_GROUP(7)},
};
const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups      = key_behavior_feedback_led_groups_data;
const uint8_t                                  key_behavior_feedback_led_group_count = (uint8_t)ARRAY_SIZE(key_behavior_feedback_led_groups_data);

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
    if (!frame) {
        return;
    }
    for (uint8_t led = led_min; led < led_max; led++) {
        frame->colors[led]  = (rgb_t){0};
        frame->painted[led] = false;
    }
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

static noah_effective_profile_identity_t test_identity(uint32_t generation) {
    return (noah_effective_profile_identity_t){
        .generation              = generation,
        .payload_crc32           = 0x1000u + generation,
        .payload_digest          = 0x2000u + generation,
        .compiled_default_digest = 0x3000u,
        .action_abi_digest       = 0x4000u,
        .kind                    = NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE,
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

static void check_all_unpainted(const rgb_runtime_frame_t *frame) {
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        check_unpainted(frame, led);
    }
}

typedef struct {
    const uint8_t                     *bytes;
    noah_effective_rgb_runtime_t      *runtime;
    noah_effective_profile_identity_t  previous;
    noah_effective_profile_identity_t  active;
    noah_effective_profile_snapshot_t *snapshot;
    size_t                             calls;
    size_t                             invalidate_on_call;
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

static key_feedback_broad_owner_slot_t group_slot_for_semantic(key_feedback_semantic_t semantic) {
    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING:
            return KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_BRANCH_PENDING;
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            return KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_COMMITTED;
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            return KEY_FEEDBACK_BROAD_OWNER_GROUP_HOLD_ACTIVE;
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
            return KEY_FEEDBACK_BROAD_OWNER_GROUP_LONG_HOLD_ACTIVE;
        case KEY_FEEDBACK_SEMANTIC_NONE:
        default:
            return KEY_FEEDBACK_BROAD_OWNER_COUNT;
    }
}

static void setup_feedback_maps(uint8_t *semantic_map, uint8_t *tap_branch_map, uint8_t *flash_visibility_bitmap, uint8_t *broad_owner_map, key_feedback_semantic_t semantic) {
    keypos_t owner = {.row = 1u, .col = 0u};

    key_feedback_semantic_map_clear(semantic_map);
    key_feedback_tap_branch_map_clear(tap_branch_map);
    key_origin_bitmap_clear(flash_visibility_bitmap);
    key_feedback_broad_owner_map_clear(broad_owner_map);
    key_feedback_semantic_map_set(semantic_map, owner, semantic);
    key_feedback_tap_branch_map_set(tap_branch_map, owner, 3u);
    if (key_feedback_semantic_is_flashing(semantic)) {
        key_origin_bitmap_add_keypos(flash_visibility_bitmap, owner);
    }
    key_feedback_broad_owner_map_set(broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, owner);
    key_feedback_broad_owner_map_set(broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF, owner);
    key_feedback_broad_owner_map_set(broad_owner_map, group_slot_for_semantic(semantic), owner);
}

static rgb_t live_color_for_semantic(key_feedback_semantic_t semantic) {
    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING:
            return hsv_to_rgb((hsv_t){.h = 100u, .s = 101u, .v = 102u});
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            return hsv_to_rgb((hsv_t){.h = 0u, .s = 0u, .v = 130u});
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            return hsv_to_rgb((hsv_t){.h = 18u, .s = 200u, .v = 200u});
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
            return hsv_to_rgb((hsv_t){.h = 148u, .s = 200u, .v = 200u});
        case KEY_FEEDBACK_SEMANTIC_NONE:
        default:
            return (rgb_t){0};
    }
}

static void check_live_key_half_and_group(const rgb_runtime_frame_t *frame, rgb_t expected) {
    check_led(frame, 0u, expected);
    check_led(frame, 1u, expected);
    check_unpainted(frame, 2u);
    check_unpainted(frame, 3u);
    for (uint8_t led = RGB_LEFT_LED_COUNT; led < RGB_MATRIX_LED_COUNT; led++) {
        check_led(frame, led, expected);
    }
}

static void test_compiled_and_live_key_rendering(const char *fixture_path) {
    static const key_feedback_semantic_t visible_semantics[] = {
        KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED, KEY_FEEDBACK_SEMANTIC_HOLD_PENDING, KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING, KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY, KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING,
    };
    uint8_t                           payload[TEST_RGB_PAYLOAD_SIZE];
    size_t                            length = fixture_payload(fixture_path, payload, sizeof(payload));
    noah_profile_rgb_v1_limits_t      limits = noah_profile_rgb_v1_default_limits();
    noah_profile_rgb_v1_view_t        view;
    noah_profile_rgb_v1_error_t       error;
    noah_effective_rgb_runtime_t      runtime;
    noah_effective_rgb_frame_t        profile_frame;
    noah_effective_profile_identity_t active   = test_identity(1u);
    noah_effective_profile_snapshot_t snapshot = {0};
    rgb_runtime_frame_t               frame;
    uint8_t                           semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t                           tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    uint8_t                           flash_visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t                           broad_owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];

    setup_feedback_maps(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_commit_mode() == KEY_FEEDBACK_TAP_COMMIT_OFF);
    CHECK(rgb_runtime_key_feedback_stage_render_effective_frame(&frame, NULL, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, 0u, RGB_MATRIX_LED_COUNT));
    check_led(&frame, 4u, hsv_to_rgb((hsv_t){.h = 21u, .s = 22u, .v = 23u}));
    check_led(&frame, 7u, hsv_to_rgb(key_behavior_feedback_led_groups[0].color));
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        if (led != 4u && led != 7u) check_unpainted(&frame, led);
    }

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
    CHECK(key_feedback_tap_commit_mode() == KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS);

    for (uint8_t index = 0u; index < ARRAY_SIZE(visible_semantics); index++) {
        key_feedback_semantic_t semantic = visible_semantics[index];

        setup_feedback_maps(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, semantic);
        CHECK(rgb_runtime_key_feedback_stage_render_effective_frame(&frame, &profile_frame, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, 0u, RGB_MATRIX_LED_COUNT));
        check_live_key_half_and_group(&frame, live_color_for_semantic(semantic));
    }

    noah_effective_rgb_frame_t disabled_frame = profile_frame;
    disabled_frame.view.stage_enable_mask &= (uint16_t)~NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR;
    CHECK(!rgb_runtime_key_feedback_stage_render_effective_frame(&frame, &disabled_frame, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, 0u, RGB_MATRIX_LED_COUNT));
    check_all_unpainted(&frame);

    noah_effective_profile_identity_t next          = test_identity(2u);
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
    CHECK(rgb_runtime_key_feedback_stage_render_effective_frame(&frame, &profile_frame, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, 0u, RGB_MATRIX_LED_COUNT));
    CHECK(reader.calls > 1u);

    size_t final_reader_call  = reader.calls;
    reader.calls              = 0u;
    reader.invalidate_on_call = final_reader_call;
    CHECK(!rgb_runtime_key_feedback_stage_render_effective_frame(&frame, &profile_frame, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, 0u, RGB_MATRIX_LED_COUNT));
    CHECK(reader.calls == final_reader_call);
    check_all_unpainted(&frame);
    noah_effective_rgb_runtime_uninstall(&runtime);
}

static void test_tap_commit_policy_fails_closed_on_stale_read(const char *fixture_path) {
    uint8_t                           payload[TEST_RGB_PAYLOAD_SIZE];
    size_t                            length = fixture_payload(fixture_path, payload, sizeof(payload));
    noah_profile_rgb_v1_limits_t      limits = noah_profile_rgb_v1_default_limits();
    noah_profile_rgb_v1_view_t        view;
    noah_profile_rgb_v1_error_t       error;
    noah_effective_rgb_runtime_t      runtime;
    noah_effective_profile_identity_t active        = test_identity(20u);
    noah_effective_profile_identity_t next          = test_identity(21u);
    noah_effective_profile_snapshot_t snapshot      = {0};
    noah_effective_profile_snapshot_t behavior_only = {.identity = next};
    invalidating_reader_t             reader        = {
        .bytes              = payload,
        .runtime            = &runtime,
        .previous           = active,
        .active             = next,
        .snapshot           = &behavior_only,
        .invalidate_on_call = 1u,
    };

    limits.logical_layer_count    = LAYER_COUNT;
    limits.maximum_brightness     = 200u;
    limits.tap_branch_color_count = 4u;
    CHECK(noah_profile_rgb_v1_decode(payload, length, &limits, &view, &error) == NOAH_PROFILE_RGB_V1_OK);
    view.reader                  = (noah_profile_reader_t){.read = invalidating_reader_read, .context = &reader, .length = length};
    snapshot.identity            = active;
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    snapshot.profile.rgb         = view;

    noah_effective_rgb_runtime_init(&runtime);
    CHECK(noah_effective_rgb_runtime_install(&runtime));
    noah_effective_rgb_runtime_invalidate(&runtime, 1u, (noah_effective_profile_identity_t){0}, active, &snapshot);
    CHECK(key_feedback_tap_commit_mode() == KEY_FEEDBACK_TAP_COMMIT_OFF);
    CHECK(reader.calls == 1u);
    noah_effective_rgb_runtime_uninstall(&runtime);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    test_compiled_and_live_key_rendering(argv[1]);
    test_tap_commit_policy_fails_closed_on_stale_read(argv[1]);
    puts("rgb effective key-feedback host tests passed");
    return 0;
}
