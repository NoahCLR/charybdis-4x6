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

#define TEST_TAP_BRANCH(h_, s_, v_) {.color = HSV(h_, s_, v_), .locality = RGB_KEYS_ONLY}
// One branch per tap past the first: four at depth five, seven at depth eight.
#if KEY_BEHAVIOR_MAX_TAP_COUNT == 8u
#    define TEST_TAP_BRANCHES RGB_TAP_BRANCHES(TEST_TAP_BRANCH(11, 12, 13), TEST_TAP_BRANCH(21, 22, 23), TEST_TAP_BRANCH(31, 32, 33), TEST_TAP_BRANCH(41, 42, 43), TEST_TAP_BRANCH(81, 82, 83), TEST_TAP_BRANCH(91, 92, 93), TEST_TAP_BRANCH(101, 102, 103))
#else
#    define TEST_TAP_BRANCHES RGB_TAP_BRANCHES(TEST_TAP_BRANCH(11, 12, 13), TEST_TAP_BRANCH(21, 22, 23), TEST_TAP_BRANCH(31, 32, 33), TEST_TAP_BRANCH(41, 42, 43))
#endif
const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {
    TEST_TAP_BRANCHES, .tap_committed_color = HSV(51, 52, 53), .tap_committed_locality = RGB_KEYS_ONLY, .hold_active_color = HSV(61, 62, 63), .hold_active_locality = RGB_KEYS_ONLY, .long_hold_active_color = HSV(71, 72, 73), .long_hold_active_locality = RGB_KEYS_ONLY, .tap_commit_mode = KEY_FEEDBACK_TAP_COMMIT_OFF,
};

static const key_behavior_feedback_led_group_t key_behavior_feedback_led_groups_data[] = {
    {.semantic = KEY_FEEDBACK_GROUP_ALL, .color = HSV(201, 202, 203), .led_group = RGB_LED_GROUP(7)},
};
const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups      = key_behavior_feedback_led_groups_data;
const uint8_t                                  key_behavior_feedback_led_group_count = (uint8_t)ARRAY_SIZE(key_behavior_feedback_led_groups_data);

// Room for the compiled encoding of this test's lighting tables.
EXPORT_RGB_COMPILED_CACHE(0u, 0u, 0u, 0u, ARRAY_SIZE(key_behavior_feedback_led_groups_data), 0u);

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

static void check_all_unpainted(const rgb_runtime_frame_t *frame) {
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        check_unpainted(frame, led);
    }
}

typedef struct {
    const uint8_t                     *bytes;
    noah_active_profile_rgb_runtime_t *runtime;
    noah_active_profile_identity_t     previous;
    noah_active_profile_identity_t     active;
    noah_active_profile_snapshot_t    *snapshot;
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
    noah_active_profile_rgb_runtime_t runtime;
    noah_active_profile_rgb_frame_t   profile_frame;
    noah_active_profile_identity_t    active   = test_identity(1u);
    noah_active_profile_snapshot_t    snapshot = {0};
    rgb_runtime_frame_t               frame;
    uint8_t                           semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t                           tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    uint8_t                           flash_visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t                           broad_owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];

    setup_feedback_maps(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_commit_mode() == KEY_FEEDBACK_TAP_COMMIT_OFF);
    CHECK(rgb_runtime_key_feedback_stage_render_active_profile_frame(&frame, NULL, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, 0u, RGB_MATRIX_LED_COUNT));
    check_led(&frame, 4u, hsv_to_rgb((hsv_t){.h = 21u, .s = 22u, .v = 23u}));
    check_led(&frame, 7u, hsv_to_rgb(key_behavior_feedback_led_groups[0].color));
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        if (led != 4u && led != 7u) check_unpainted(&frame, led);
    }

    // This rendering fixture models six supported slots with current RGB v3.
    limits.supported_pointing_mode_mask = 0x3fu;
    limits.logical_layer_count          = LAYER_COUNT;
    limits.maximum_brightness           = 200u;
    limits.tap_branch_color_count       = 4u;
    CHECK(noah_profile_rgb_v1_decode(payload, length, &limits, &view, &error) == NOAH_PROFILE_RGB_V1_OK);
    snapshot.identity            = active;
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    snapshot.profile.rgb         = view;

    noah_active_profile_rgb_runtime_init(&runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&runtime));
    noah_active_profile_rgb_runtime_invalidate(&runtime, 1u, (noah_active_profile_identity_t){0}, active, &snapshot);
    CHECK(rgb_active_profile_config_capture_frame(&profile_frame) == NOAH_ACTIVE_PROFILE_RGB_OK);
    CHECK(key_feedback_tap_commit_mode() == KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS);

    for (uint8_t index = 0u; index < ARRAY_SIZE(visible_semantics); index++) {
        key_feedback_semantic_t semantic = visible_semantics[index];

        setup_feedback_maps(semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, semantic);
        CHECK(rgb_runtime_key_feedback_stage_render_active_profile_frame(&frame, &profile_frame, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, 0u, RGB_MATRIX_LED_COUNT));
        check_live_key_half_and_group(&frame, live_color_for_semantic(semantic));
    }

    noah_active_profile_rgb_frame_t disabled_frame = profile_frame;
    disabled_frame.view.stage_enable_mask &= (uint16_t)~NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR;
    CHECK(!rgb_runtime_key_feedback_stage_render_active_profile_frame(&frame, &disabled_frame, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, 0u, RGB_MATRIX_LED_COUNT));
    check_all_unpainted(&frame);

    noah_active_profile_identity_t next          = test_identity(2u);
    noah_active_profile_snapshot_t behavior_only = {.identity = next};
    invalidating_reader_t          reader        = {
        .bytes              = payload,
        .runtime            = &runtime,
        .previous           = active,
        .active             = next,
        .snapshot           = &behavior_only,
        .invalidate_on_call = SIZE_MAX,
    };
    profile_frame.view.reader = (noah_profile_reader_t){.read = invalidating_reader_read, .context = &reader, .length = length};
    CHECK(rgb_runtime_key_feedback_stage_render_active_profile_frame(&frame, &profile_frame, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, 0u, RGB_MATRIX_LED_COUNT));
    CHECK(reader.calls > 1u);

    size_t final_reader_call  = reader.calls;
    reader.calls              = 0u;
    reader.invalidate_on_call = final_reader_call;
    CHECK(!rgb_runtime_key_feedback_stage_render_active_profile_frame(&frame, &profile_frame, semantic_map, tap_branch_map, flash_visibility_bitmap, broad_owner_map, 0u, RGB_MATRIX_LED_COUNT));
    CHECK(reader.calls == final_reader_call);
    check_all_unpainted(&frame);
    noah_active_profile_rgb_runtime_uninstall(&runtime);
}

static void test_tap_commit_policy_fails_closed_on_stale_read(const char *fixture_path) {
    uint8_t                           payload[TEST_RGB_PAYLOAD_SIZE];
    size_t                            length = fixture_payload(fixture_path, payload, sizeof(payload));
    noah_profile_rgb_v1_limits_t      limits = noah_profile_rgb_v1_default_limits();
    noah_profile_rgb_v1_view_t        view;
    noah_profile_rgb_v1_error_t       error;
    noah_active_profile_rgb_runtime_t runtime;
    noah_active_profile_identity_t    active        = test_identity(20u);
    noah_active_profile_identity_t    next          = test_identity(21u);
    noah_active_profile_snapshot_t    snapshot      = {0};
    noah_active_profile_snapshot_t    behavior_only = {.identity = next};
    invalidating_reader_t             reader        = {
        .bytes              = payload,
        .runtime            = &runtime,
        .previous           = active,
        .active             = next,
        .snapshot           = &behavior_only,
        .invalidate_on_call = 1u,
    };

    // This rendering fixture models six supported slots with current RGB v3.
    limits.supported_pointing_mode_mask = 0x3fu;
    limits.logical_layer_count          = LAYER_COUNT;
    limits.maximum_brightness           = 200u;
    limits.tap_branch_color_count       = 4u;
    CHECK(noah_profile_rgb_v1_decode(payload, length, &limits, &view, &error) == NOAH_PROFILE_RGB_V1_OK);
    view.reader                  = (noah_profile_reader_t){.read = invalidating_reader_read, .context = &reader, .length = length};
    snapshot.identity            = active;
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    snapshot.profile.rgb         = view;

    noah_active_profile_rgb_runtime_init(&runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&runtime));
    noah_active_profile_rgb_runtime_invalidate(&runtime, 1u, (noah_active_profile_identity_t){0}, active, &snapshot);
    CHECK(key_feedback_tap_commit_mode() == KEY_FEEDBACK_TAP_COMMIT_OFF);
    CHECK(reader.calls == 1u);
    noah_active_profile_rgb_runtime_uninstall(&runtime);
}

// ── Each case its own locality (D-F73) ──────────────────────────────────────
//
// Keys are a 2 x 4 matrix, row 0 the left half (LEDs 0..3), row 1 the right
// half (LEDs 4..7); a key's LED is its matrix index.

// The cases in case order: tap counts 2 to 5, tap committed, hold, long hold.
enum { CASE_TAP_2, CASE_TAP_3, CASE_TAP_4, CASE_TAP_5, CASE_COMMITTED, CASE_HOLD, CASE_LONG_HOLD, CASE_COUNT };

static const hsv_t case_colors[CASE_COUNT] = {{111u, 1u, 2u}, {112u, 1u, 2u}, {113u, 1u, 2u}, {114u, 1u, 2u}, {115u, 1u, 2u}, {116u, 1u, 2u}, {117u, 1u, 2u}};

typedef struct {
    uint8_t of[CASE_COUNT];
} case_localities_t;

static case_localities_t every_case(uint8_t locality) {
    case_localities_t localities;
    memset(localities.of, locality, sizeof(localities.of));
    return localities;
}

typedef struct {
    uint8_t selector;
    hsv_t   color;
    uint8_t led_mask;
} live_row_t;

enum { LIVE_PAYLOAD_MAX = 256 };

static noah_active_profile_rgb_runtime_t live_runtime;
static uint8_t                           live_bytes[LIVE_PAYLOAD_MAX];
static uint32_t                          live_generation = 100u;

// A lighting domain with the layer and behaviour-feedback stages: one LED set
// per distinct row mask, four tap branches and the tiers in these colours
// (in case order), a locality for each case, and the rows.
static noah_active_profile_rgb_frame_t live_profile_coloured(const hsv_t *colors, case_localities_t localities, const live_row_t *rows, uint8_t row_count) {
    noah_profile_rgb_v1_limits_t    limits   = noah_profile_rgb_v1_default_limits();
    noah_active_profile_snapshot_t  snapshot = {0};
    noah_active_profile_rgb_frame_t frame;
    noah_profile_rgb_v1_error_t     error;
    uint8_t                         masks[8];
    uint8_t                         groups = 0u;
    size_t                          offset;

    for (uint16_t mask = 0u; mask <= UINT8_MAX; mask++) {
        for (uint8_t row = 0u; row < row_count; row++) {
            if (rows[row].led_mask == mask && (groups == 0u || masks[groups - 1u] != mask)) masks[groups++] = (uint8_t)mask;
        }
    }
    memset(live_bytes, 0, sizeof(live_bytes));
    live_bytes[0]  = NOAH_PROFILE_RGB_V1_FORMAT_VERSION;
    live_bytes[2]  = NOAH_PROFILE_RGB_V1_STAGE_LAYER | NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR;
    live_bytes[4]  = groups;
    live_bytes[5]  = LAYER_COUNT;
    live_bytes[10] = 4u;
    live_bytes[11] = row_count;
    live_bytes[12] = NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT;
    live_bytes[13] = NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE;
    offset         = NOAH_PROFILE_RGB_V1_HEADER_SIZE;
    for (uint8_t group = 0u; group < groups; group++, offset += NOAH_PROFILE_RGB_V1_GROUP_RECORD_SIZE) {
        live_bytes[offset]      = group;
        live_bytes[offset + 1u] = masks[group];
    }
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++, offset += NOAH_PROFILE_RGB_V1_LAYER_COLOR_RECORD_SIZE) {
        live_bytes[offset] = layer;
    }
    offset += NOAH_PROFILE_RGB_V1_AUTOMOUSE_RECORD_SIZE + NOAH_PROFILE_RGB_V1_COMBO_RECORD_SIZE;
    for (uint8_t branch = 0u; branch < 4u; branch++, offset += NOAH_PROFILE_RGB_V1_TAP_BRANCH_RECORD_SIZE) {
        memcpy(&live_bytes[offset], (const uint8_t[]){colors[branch].h, colors[branch].s, colors[branch].v, localities.of[branch]}, NOAH_PROFILE_RGB_V1_TAP_BRANCH_RECORD_SIZE);
    }
    memcpy(&live_bytes[offset], (const uint8_t[]){colors[CASE_COMMITTED].h, colors[CASE_COMMITTED].s, colors[CASE_COMMITTED].v, colors[CASE_HOLD].h, colors[CASE_HOLD].s, colors[CASE_HOLD].v, colors[CASE_LONG_HOLD].h, colors[CASE_LONG_HOLD].s, colors[CASE_LONG_HOLD].v, 0u, localities.of[CASE_COMMITTED], localities.of[CASE_HOLD], localities.of[CASE_LONG_HOLD]}, NOAH_PROFILE_RGB_V1_KEY_FEEDBACK_SIZE);
    offset += NOAH_PROFILE_RGB_V1_KEY_FEEDBACK_SIZE;
    for (uint8_t row = 0u; row < row_count; row++, offset += NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE) {
        uint8_t group = 0u;

        while (masks[group] != rows[row].led_mask)
            group++;
        memcpy(&live_bytes[offset], (const uint8_t[]){rows[row].selector, rows[row].color.h, rows[row].color.s, rows[row].color.v, group}, NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE);
    }
    offset += NOAH_PROFILE_RGB_V1_AUTOCORRECT_FEEDBACK_SIZE;
    CHECK(offset <= sizeof(live_bytes));

    limits.compiled_stage_mask          = NOAH_PROFILE_RGB_V1_STAGE_LAYER | NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR;
    limits.supported_pointing_mode_mask = 0u;
    limits.logical_layer_count          = LAYER_COUNT;
    limits.maximum_brightness           = 200u;
    limits.tap_branch_color_count       = 4u;
    live_generation++;
    snapshot.identity            = test_identity(live_generation);
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    CHECK(noah_profile_rgb_v1_decode(live_bytes, offset, &limits, &snapshot.profile.rgb, &error) == NOAH_PROFILE_RGB_V1_OK);
    noah_active_profile_rgb_runtime_invalidate(&live_runtime, live_generation, (noah_active_profile_identity_t){0}, snapshot.identity, &snapshot);
    CHECK(rgb_active_profile_config_capture_frame(&frame) == NOAH_ACTIVE_PROFILE_RGB_OK);
    return frame;
}

static noah_active_profile_rgb_frame_t live_profile(case_localities_t localities, const live_row_t *rows, uint8_t row_count) {
    return live_profile_coloured(case_colors, localities, rows, row_count);
}

// A key showing one state. A flashing state is lit unless `dark`.
typedef struct {
    uint8_t                 key;
    key_feedback_semantic_t semantic;
    uint8_t                 tap_count;
    bool                    dark;
} key_state_t;

typedef struct {
    uint8_t semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    uint8_t flash_visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t broad_owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
} feedback_maps_t;

static keypos_t key_at(uint8_t key) {
    return (keypos_t){.row = (uint8_t)(key / MATRIX_COLS), .col = (uint8_t)(key % MATRIX_COLS)};
}

// The maps the key runtime would send for these keys, oldest first: the
// newest key overall, on each half and of each tier owns its slot.
static feedback_maps_t maps_for(const key_state_t *states, uint8_t count) {
    feedback_maps_t maps;

    key_feedback_semantic_map_clear(maps.semantic_map);
    key_feedback_tap_branch_map_clear(maps.tap_branch_map);
    key_origin_bitmap_clear(maps.flash_visibility_bitmap);
    key_feedback_broad_owner_map_clear(maps.broad_owner_map);
    for (uint8_t index = 0u; index < count; index++) {
        keypos_t key_pos = key_at(states[index].key);

        key_feedback_semantic_map_set(maps.semantic_map, key_pos, states[index].semantic);
        if (states[index].semantic == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING) {
            key_feedback_tap_branch_map_set(maps.tap_branch_map, key_pos, states[index].tap_count);
        }
        if (key_feedback_semantic_is_flashing(states[index].semantic) && !states[index].dark) {
            key_origin_bitmap_add_keypos(maps.flash_visibility_bitmap, key_pos);
        }
        key_feedback_broad_owner_map_set(maps.broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, key_pos);
        key_feedback_broad_owner_map_set(maps.broad_owner_map, key_pos.row == 0u ? KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF : KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF, key_pos);
        key_feedback_broad_owner_map_set(maps.broad_owner_map, group_slot_for_semantic(states[index].semantic), key_pos);
    }
    return maps;
}

static bool render_maps(rgb_runtime_frame_t *frame, const noah_active_profile_rgb_frame_t *profile, const feedback_maps_t *maps) {
    return rgb_runtime_key_feedback_stage_render_active_profile_frame(frame, profile, maps->semantic_map, maps->tap_branch_map, maps->flash_visibility_bitmap, maps->broad_owner_map, 0u, RGB_MATRIX_LED_COUNT);
}

static uint8_t case_of(const key_state_t *state) {
    switch (state->semantic) {
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING:
            return state->tap_count <= 2u ? CASE_TAP_2 : (state->tap_count >= 5u ? CASE_TAP_5 : (uint8_t)(state->tap_count - 2u));
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            return CASE_COMMITTED;
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            return CASE_HOLD;
        default:
            return CASE_LONG_HOLD;
    }
}

static bool lit(const key_state_t *state) {
    return !(key_feedback_semantic_is_flashing(state->semantic) && state->dark);
}

// What a frame should show, LED by LED; `none` for an LED left unpainted.
typedef struct {
    bool  painted[RGB_MATRIX_LED_COUNT];
    rgb_t colors[RGB_MATRIX_LED_COUNT];
} expected_frame_t;

static void expect_leds(expected_frame_t *expected, uint8_t from, uint8_t to, hsv_t color) {
    for (uint8_t led = from; led < to; led++) {
        expected->painted[led] = true;
        expected->colors[led]  = hsv_to_rgb(color);
    }
}

static void check_expected(const rgb_runtime_frame_t *frame, const expected_frame_t *expected) {
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        CHECK(frame->painted[led] == expected->painted[led]);
        if (expected->painted[led]) {
            CHECK(frame->colors[led].r == expected->colors[led].r && frame->colors[led].g == expected->colors[led].g && frame->colors[led].b == expected->colors[led].b);
        }
    }
}

// What the stage painted when behaviour feedback had one locality: each key
// on its own key under the keys locality; otherwise the newest key's state
// (on each half, for the half of the key) on its area, nothing while it is in
// its flash's dark phase.
static expected_frame_t one_locality_frame(uint8_t locality, const key_state_t *states, uint8_t count) {
    expected_frame_t expected = {0};

    if (locality == RGB_KEYS_ONLY) {
        for (uint8_t index = 0u; index < count; index++) {
            if (lit(&states[index])) expect_leds(&expected, states[index].key, (uint8_t)(states[index].key + 1u), case_colors[case_of(&states[index])]);
        }
        return expected;
    }
    if (locality == RGB_KEY_HALF) {
        for (uint8_t half = 0u; half < 2u; half++) {
            const key_state_t *newest = NULL;

            for (uint8_t index = 0u; index < count; index++) {
                if (states[index].key / MATRIX_COLS == half) newest = &states[index];
            }
            if (newest && lit(newest)) expect_leds(&expected, (uint8_t)(half * RGB_LEFT_LED_COUNT), (uint8_t)((half + 1u) * RGB_LEFT_LED_COUNT), case_colors[case_of(newest)]);
        }
        return expected;
    }
    if (count > 0u && lit(&states[count - 1u])) {
        hsv_t color = case_colors[case_of(&states[count - 1u])];

        if (locality == RGB_BOTH_HALVES) expect_leds(&expected, 0u, RGB_MATRIX_LED_COUNT, color);
        if (locality == RGB_LEFT_HALF) expect_leds(&expected, 0u, RGB_LEFT_LED_COUNT, color);
        if (locality == RGB_RIGHT_HALF) expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, color);
    }
    return expected;
}

// With every case in one locality the stage paints what it painted with one
// locality, for mixed keys on both halves: several cases at once, a newest key
// in its flash's dark phase over older lit ones, a tap count past the last
// branch, and a half with no key (D-F73).
static void test_one_locality_paints_as_before(void) {
    static const key_state_t mixed[] = {
        {.key = 1u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING},
        {.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 3u},
        {.key = 3u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY},
    };
    static const key_state_t dark_newest[] = {
        {.key = 0u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u},
        {.key = 7u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED},
        {.key = 2u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY},
        {.key = 4u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING, .dark = true},
    };
    static const key_state_t one_half[] = {
        {.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 6u},
        {.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING},
    };
    static const key_state_t dark_left[] = {
        {.key = 1u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY},
        {.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 4u},
        {.key = 2u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING, .dark = true},
        {.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 5u},
    };
    static const struct {
        const key_state_t *states;
        uint8_t            count;
    } scenes[] = {{mixed, ARRAY_SIZE(mixed)}, {dark_newest, ARRAY_SIZE(dark_newest)}, {one_half, ARRAY_SIZE(one_half)}, {dark_left, ARRAY_SIZE(dark_left)}};

    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    for (uint8_t locality = RGB_BOTH_HALVES; locality <= RGB_KEYS_ONLY; locality++) {
        noah_active_profile_rgb_frame_t profile = live_profile(every_case(locality), NULL, 0u);

        for (uint8_t scene = 0u; scene < ARRAY_SIZE(scenes); scene++) {
            for (uint8_t count = 1u; count <= scenes[scene].count; count++) {
                feedback_maps_t     maps     = maps_for(scenes[scene].states, count);
                expected_frame_t    expected = one_locality_frame(locality, scenes[scene].states, count);
                rgb_runtime_frame_t frame;

                (void)render_maps(&frame, &profile, &maps);
                check_expected(&frame, &expected);
            }
        }
    }
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
}

// Cases in different localities show at once, widest first: a hold on both
// halves under a tap branch on its key; a long hold on the left half beside
// tap committed on its key's half; a fixed half under the half of the key.
static void test_a_narrower_case_paints_over_a_wider_one(void) {
    case_localities_t localities = every_case(RGB_KEY_HALF);

    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    localities.of[CASE_HOLD]  = RGB_BOTH_HALVES;
    localities.of[CASE_TAP_2] = RGB_KEYS_ONLY;
    {
        noah_active_profile_rgb_frame_t profile = live_profile(localities, NULL, 0u);
        // The tap branch on its key over the hold, the hold newest or not.
        static const key_state_t newer_tap[]  = {{.key = 1u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING}, {.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u}};
        static const key_state_t newer_hold[] = {{.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u}, {.key = 1u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING}};
        // A flashing hold in its dark phase shows the tap branch alone.
        static const key_state_t dark_hold[] = {{.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u}, {.key = 1u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING, .dark = true}};
        expected_frame_t         expected    = {0};
        feedback_maps_t          maps;
        rgb_runtime_frame_t      frame;

        expect_leds(&expected, 0u, RGB_MATRIX_LED_COUNT, case_colors[CASE_HOLD]);
        expect_leds(&expected, 6u, 7u, case_colors[CASE_TAP_2]);
        maps = maps_for(newer_tap, ARRAY_SIZE(newer_tap));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
        maps = maps_for(newer_hold, ARRAY_SIZE(newer_hold));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);

        memset(&expected, 0, sizeof(expected));
        expect_leds(&expected, 6u, 7u, case_colors[CASE_TAP_2]);
        maps = maps_for(dark_hold, ARRAY_SIZE(dark_hold));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
    }

    localities                    = every_case(RGB_KEY_HALF);
    localities.of[CASE_LONG_HOLD] = RGB_LEFT_HALF;
    {
        noah_active_profile_rgb_frame_t profile = live_profile(localities, NULL, 0u);
        // A long hold on a right key paints the left half; tap committed on
        // another right key paints the right half.
        static const key_state_t sides[] = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY}, {.key = 7u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED}};
        // Both on the left: the key's half paints over the fixed half.
        static const key_state_t left[]   = {{.key = 0u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED}, {.key = 2u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY}};
        expected_frame_t         expected = {0};
        feedback_maps_t          maps;
        rgb_runtime_frame_t      frame;

        expect_leds(&expected, 0u, RGB_LEFT_LED_COUNT, case_colors[CASE_LONG_HOLD]);
        expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, case_colors[CASE_COMMITTED]);
        maps = maps_for(sides, ARRAY_SIZE(sides));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);

        memset(&expected, 0, sizeof(expected));
        expect_leds(&expected, 0u, RGB_LEFT_LED_COUNT, case_colors[CASE_COMMITTED]);
        maps = maps_for(left, ARRAY_SIZE(left));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
    }
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
}

// Where cases share an area the newest key's case decides it, through its
// flash's dark phase too; when the newest key shows a case of another
// locality, the later case in case order paints over the earlier, each only
// while it shows.
static void test_the_newest_key_decides_a_shared_area(void) {
    case_localities_t localities = every_case(RGB_KEY_HALF);

    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    localities.of[CASE_TAP_2] = RGB_KEYS_ONLY;
    {
        noah_active_profile_rgb_frame_t profile           = live_profile(localities, NULL, 0u);
        static const key_state_t        long_hold_newer[] = {{.key = 4u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING}, {.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY}};
        static const key_state_t        hold_newer[]      = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY}, {.key = 4u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING}};
        static const key_state_t        dark_newest[]     = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY}, {.key = 4u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING, .dark = true}};
        // The newest key shows a tap branch on its key alone: the later of
        // the half's cases, long hold, paints the half whichever is older.
        static const key_state_t tap_newest[]      = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY}, {.key = 4u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING}, {.key = 7u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u}};
        static const key_state_t tap_newest_dark[] = {{.key = 4u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING}, {.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING, .dark = true}, {.key = 7u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u}};
        expected_frame_t         expected          = {0};
        feedback_maps_t          maps;
        rgb_runtime_frame_t      frame;

        expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, case_colors[CASE_LONG_HOLD]);
        maps = maps_for(long_hold_newer, ARRAY_SIZE(long_hold_newer));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);

        memset(&expected, 0, sizeof(expected));
        expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, case_colors[CASE_HOLD]);
        maps = maps_for(hold_newer, ARRAY_SIZE(hold_newer));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);

        // The newest hold's dark phase darkens its half, not the older long hold.
        memset(&expected, 0, sizeof(expected));
        maps = maps_for(dark_newest, ARRAY_SIZE(dark_newest));
        CHECK(!render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);

        memset(&expected, 0, sizeof(expected));
        expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, case_colors[CASE_LONG_HOLD]);
        expect_leds(&expected, 7u, 8u, case_colors[CASE_TAP_2]);
        maps = maps_for(tap_newest, ARRAY_SIZE(tap_newest));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);

        memset(&expected, 0, sizeof(expected));
        expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, case_colors[CASE_HOLD]);
        expect_leds(&expected, 7u, 8u, case_colors[CASE_TAP_2]);
        maps = maps_for(tap_newest_dark, ARRAY_SIZE(tap_newest_dark));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
    }
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
}

// Each tap branch is a case: its own colour and locality, and an accent that
// paints while any key shows that branch, whichever key is newest; a deeper
// tap count takes the last branch's (D-F73).
static void test_each_tap_branch_is_a_case(void) {
    static const live_row_t rows[] = {
        {.selector = NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_3, .color = {0u, 0u, 0u}, .led_mask = 1u << 0u},
        {.selector = NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_2, .color = {201u, 202u, 103u}, .led_mask = 1u << 1u},
        {.selector = NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_5, .color = {0u, 0u, 0u}, .led_mask = 1u << 2u},
        {.selector = NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_HOLD, .color = {0u, 0u, 0u}, .led_mask = 1u << 3u},
    };
    case_localities_t localities = every_case(RGB_KEYS_ONLY);

    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    localities.of[CASE_TAP_3] = RGB_RIGHT_HALF;
    {
        noah_active_profile_rgb_frame_t profile = live_profile(localities, rows, ARRAY_SIZE(rows));
        // An older triple tap and a newer double tap: each paints by its own
        // locality, and only their own accents paint.
        static const key_state_t both[]   = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 3u}, {.key = 7u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u}};
        static const key_state_t triple[] = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 3u}};
        // Tap count 6 is past the last branch: tap count 5's case and accent.
        static const key_state_t deeper[] = {{.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 6u}};
        expected_frame_t         expected = {0};
        feedback_maps_t          maps;
        rgb_runtime_frame_t      frame;

        expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, case_colors[CASE_TAP_3]);
        expect_leds(&expected, 7u, 8u, case_colors[CASE_TAP_2]);
        expect_leds(&expected, 0u, 1u, case_colors[CASE_TAP_3]);
        expect_leds(&expected, 1u, 2u, (hsv_t){201u, 202u, 103u});
        maps = maps_for(both, ARRAY_SIZE(both));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);

        memset(&expected, 0, sizeof(expected));
        expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, case_colors[CASE_TAP_3]);
        expect_leds(&expected, 0u, 1u, case_colors[CASE_TAP_3]);
        maps = maps_for(triple, ARRAY_SIZE(triple));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);

        memset(&expected, 0, sizeof(expected));
        expect_leds(&expected, 6u, 7u, case_colors[CASE_TAP_5]);
        expect_leds(&expected, 2u, 3u, case_colors[CASE_TAP_5]);
        maps = maps_for(deeper, ARRAY_SIZE(deeper));
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
    }
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
}

// A frame of one LED range, as each half renders its own: LEDs outside it keep
// what the frame held.
static bool render_maps_range(rgb_runtime_frame_t *frame, const noah_active_profile_rgb_frame_t *profile, const feedback_maps_t *maps, uint8_t led_min, uint8_t led_max) {
    return rgb_runtime_key_feedback_stage_render_active_profile_frame(frame, profile, maps->semantic_map, maps->tap_branch_map, maps->flash_visibility_bitmap, maps->broad_owner_map, led_min, led_max);
}

static void check_expected_range(const rgb_runtime_frame_t *frame, const expected_frame_t *expected, uint8_t led_min, uint8_t led_max) {
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        if (led < led_min || led >= led_max) {
            // Untouched: still the 0x5a the frame was filled with.
            CHECK(frame->colors[led].r == 0x5au);
            continue;
        }
        CHECK(frame->painted[led] == expected->painted[led]);
        if (expected->painted[led]) {
            CHECK(frame->colors[led].r == expected->colors[led].r && frame->colors[led].g == expected->colors[led].g && frame->colors[led].b == expected->colors[led].b);
        }
    }
}

// Every case under a locality other than the half of the key, beside another
// case: tap count 4 on the left half under tap committed on both halves, tap
// count 5 on its key beside a long hold on the right half, and tap committed
// on its key beside a hold on the left half. The first also renders as each
// half does, its own LEDs alone.
static void test_each_case_beside_another(void) {
    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    {
        case_localities_t localities = every_case(RGB_KEY_HALF);

        localities.of[CASE_TAP_4]                = RGB_LEFT_HALF;
        localities.of[CASE_COMMITTED]            = RGB_BOTH_HALVES;
        noah_active_profile_rgb_frame_t profile  = live_profile(localities, NULL, 0u);
        static const key_state_t        scene[]  = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 4u}, {.key = 2u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED}};
        expected_frame_t                expected = {0};
        feedback_maps_t                 maps     = maps_for(scene, ARRAY_SIZE(scene));
        rgb_runtime_frame_t             frame;

        expect_leds(&expected, 0u, RGB_MATRIX_LED_COUNT, case_colors[CASE_COMMITTED]);
        expect_leds(&expected, 0u, RGB_LEFT_LED_COUNT, case_colors[CASE_TAP_4]);
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
        for (uint8_t half = 0u; half < 2u; half++) {
            uint8_t led_min = half == 0u ? 0u : RGB_LEFT_LED_COUNT;
            uint8_t led_max = half == 0u ? RGB_LEFT_LED_COUNT : RGB_MATRIX_LED_COUNT;

            memset(&frame, 0x5a, sizeof(frame));
            CHECK(render_maps_range(&frame, &profile, &maps, led_min, led_max));
            check_expected_range(&frame, &expected, led_min, led_max);
        }
    }
    {
        case_localities_t localities = every_case(RGB_KEY_HALF);

        localities.of[CASE_TAP_5]                = RGB_KEYS_ONLY;
        localities.of[CASE_LONG_HOLD]            = RGB_RIGHT_HALF;
        noah_active_profile_rgb_frame_t profile  = live_profile(localities, NULL, 0u);
        static const key_state_t        scene[]  = {{.key = 1u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 5u}, {.key = 0u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY}};
        expected_frame_t                expected = {0};
        feedback_maps_t                 maps     = maps_for(scene, ARRAY_SIZE(scene));
        rgb_runtime_frame_t             frame;

        expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, case_colors[CASE_LONG_HOLD]);
        expect_leds(&expected, 1u, 2u, case_colors[CASE_TAP_5]);
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
    }
    {
        case_localities_t localities = every_case(RGB_KEY_HALF);

        localities.of[CASE_COMMITTED]            = RGB_KEYS_ONLY;
        localities.of[CASE_HOLD]                 = RGB_LEFT_HALF;
        noah_active_profile_rgb_frame_t profile  = live_profile(localities, NULL, 0u);
        static const key_state_t        scene[]  = {{.key = 7u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING}, {.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED}};
        expected_frame_t                expected = {0};
        feedback_maps_t                 maps     = maps_for(scene, ARRAY_SIZE(scene));
        rgb_runtime_frame_t             frame;

        expect_leds(&expected, 0u, RGB_LEFT_LED_COUNT, case_colors[CASE_HOLD]);
        expect_leds(&expected, 6u, 7u, case_colors[CASE_COMMITTED]);
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
    }
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
}

// A newest key that shows no case, as an owner whose footprint the semantic
// map lacks or a slave frame between packets can name, keeps its areas dark
// rather than showing an older case; with no owner named, the cases of the
// area paint. A case on its key alone paints either way.
static void test_an_owner_showing_no_case_keeps_its_area_dark(void) {
    static const key_state_t scene[] = {
        {.key = 1u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING},
        {.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY},
        {.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u},
    };
    case_localities_t localities = every_case(RGB_KEY_HALF);

    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    localities.of[CASE_HOLD]  = RGB_BOTH_HALVES;
    localities.of[CASE_TAP_2] = RGB_KEYS_ONLY;
    {
        noah_active_profile_rgb_frame_t profile  = live_profile(localities, NULL, 0u);
        feedback_maps_t                 maps     = maps_for(scene, ARRAY_SIZE(scene));
        expected_frame_t                expected = {0};
        rgb_runtime_frame_t             frame;

        // Key 7, on the right, owns everywhere and the right half, and shows nothing.
        key_feedback_broad_owner_map_set(maps.broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, key_at(7u));
        key_feedback_broad_owner_map_set(maps.broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF, key_at(7u));
        expect_leds(&expected, 6u, 7u, case_colors[CASE_TAP_2]);
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);

        // No owner named: the hold paints both halves, the long hold its half.
        key_feedback_broad_owner_map_set(maps.broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, NOAH_KEY_RUNTIME_NO_KEY);
        key_feedback_broad_owner_map_set(maps.broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF, NOAH_KEY_RUNTIME_NO_KEY);
        memset(&expected, 0, sizeof(expected));
        expect_leds(&expected, 0u, RGB_MATRIX_LED_COUNT, case_colors[CASE_HOLD]);
        expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, case_colors[CASE_LONG_HOLD]);
        expect_leds(&expected, 6u, 7u, case_colors[CASE_TAP_2]);
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
    }
    // With every case on the half of the key it is the half's dark, as one
    // locality painted it.
    {
        noah_active_profile_rgb_frame_t profile  = live_profile(every_case(RGB_KEY_HALF), NULL, 0u);
        feedback_maps_t                 maps     = maps_for(scene, ARRAY_SIZE(scene));
        expected_frame_t                expected = {0};
        rgb_runtime_frame_t             frame;

        key_feedback_broad_owner_map_set(maps.broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF, key_at(7u));
        expect_leds(&expected, 0u, RGB_LEFT_LED_COUNT, case_colors[CASE_HOLD]);
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
    }
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
}

// A 0xff row takes the tap branches as one case, the newest key's branch: a
// black row in that branch's colour whichever older key sits on another, then
// the tiers in case order, as one tap branch semantic painted it (D-F73).
static void test_an_all_row_follows_the_newest_tap_branch(void) {
    static const live_row_t  rows[]         = {{.selector = NOAH_PROFILE_RGB_V1_SELECTOR_ALL, .color = {0u, 0u, 0u}, .led_mask = 1u << 2u}};
    static const key_state_t newer_double[] = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 3u}, {.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u}};
    static const key_state_t newer_triple[] = {{.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u}, {.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 3u}};
    static const key_state_t with_hold[]    = {{.key = 4u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING}, {.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 3u}};

    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    {
        noah_active_profile_rgb_frame_t profile = live_profile(every_case(RGB_KEYS_ONLY), rows, ARRAY_SIZE(rows));
        feedback_maps_t                 maps;
        rgb_runtime_frame_t             frame;

        maps = maps_for(newer_double, ARRAY_SIZE(newer_double));
        CHECK(render_maps(&frame, &profile, &maps));
        CHECK(frame.painted[2] && frame.colors[2].r == case_colors[CASE_TAP_2].h);
        maps = maps_for(newer_triple, ARRAY_SIZE(newer_triple));
        CHECK(render_maps(&frame, &profile, &maps));
        CHECK(frame.painted[2] && frame.colors[2].r == case_colors[CASE_TAP_3].h);
        // Through tap counts 3 to 5, and past the last branch.
        for (uint8_t tap_count = 3u; tap_count <= 6u; tap_count++) {
            const key_state_t one[] = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = tap_count}};

            maps = maps_for(one, ARRAY_SIZE(one));
            CHECK(render_maps(&frame, &profile, &maps));
            CHECK(frame.painted[2] && frame.colors[2].r == case_colors[tap_count >= 5u ? CASE_TAP_5 : CASE_TAP_2 + tap_count - 2u].h);
        }
        // A tier paints over the tap branch, later in case order.
        maps = maps_for(with_hold, ARRAY_SIZE(with_hold));
        CHECK(render_maps(&frame, &profile, &maps));
        CHECK(frame.painted[2] && frame.colors[2].r == case_colors[CASE_HOLD].h);
    }
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
}

// With no key showing any case the stage returns at once: nothing paints,
// coloured 0xff rows included, even with a newest key still named (the slave
// half between packets).
static void test_nothing_showing_paints_nothing(void) {
    static const live_row_t  rows[] = {{.selector = NOAH_PROFILE_RGB_V1_SELECTOR_ALL, .color = {171u, 172u, 173u}, .led_mask = 1u << 2u}};
    static const key_state_t hold[] = {{.key = 4u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING}};

    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    {
        noah_active_profile_rgb_frame_t profile = live_profile(every_case(RGB_BOTH_HALVES), rows, ARRAY_SIZE(rows));
        feedback_maps_t                 maps    = maps_for(hold, ARRAY_SIZE(hold));
        rgb_runtime_frame_t             frame;

        key_feedback_semantic_map_clear(maps.semantic_map);
        CHECK(!render_maps(&frame, &profile, &maps));
        for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
            CHECK(!frame.painted[led]);
        }
    }
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
}

// A row naming any tap branch (D-F75) follows the newest key with a tap
// branch pending, black in that branch's colour, its own colour otherwise; it
// paints for no tier. It paints after the 0xff rows and before the rows
// naming one case, so a narrower row repaints a wider one.
static void test_an_any_tap_branch_row_follows_the_newest_tap_branch(void) {
    static const live_row_t rows[] = {
        {.selector = NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_2, .color = {141u, 142u, 143u}, .led_mask = 1u << 4u},
        {.selector = NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_ANY, .color = {151u, 152u, 153u}, .led_mask = 1u << 4u},
        {.selector = NOAH_PROFILE_RGB_V1_SELECTOR_ALL, .color = {161u, 162u, 163u}, .led_mask = 1u << 4u},
        {.selector = NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_ANY, .color = {0u, 0u, 0u}, .led_mask = 1u << 2u},
        {.selector = NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_TAP_BRANCH_ANY, .color = {131u, 132u, 133u}, .led_mask = 1u << 3u},
    };
    static const key_state_t newer_double[] = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 3u}, {.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u}};
    static const key_state_t triple_only[]  = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 3u}};
    static const key_state_t hold_only[]    = {{.key = 4u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING}};

    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    {
        noah_active_profile_rgb_frame_t profile = live_profile(every_case(RGB_KEYS_ONLY), rows, ARRAY_SIZE(rows));
        feedback_maps_t                 maps;
        rgb_runtime_frame_t             frame;

        maps = maps_for(newer_double, ARRAY_SIZE(newer_double));
        CHECK(render_maps(&frame, &profile, &maps));
        CHECK(frame.painted[2] && frame.colors[2].r == case_colors[CASE_TAP_2].h);
        CHECK(frame.painted[3] && frame.colors[3].r == 131u);
        CHECK(frame.painted[4] && frame.colors[4].r == 141u);
        maps = maps_for(triple_only, ARRAY_SIZE(triple_only));
        CHECK(render_maps(&frame, &profile, &maps));
        CHECK(frame.painted[2] && frame.colors[2].r == case_colors[CASE_TAP_3].h);
        CHECK(frame.painted[4] && frame.colors[4].r == 151u);
        // Through tap counts 3 to 5, and past the last branch.
        for (uint8_t tap_count = 3u; tap_count <= 6u; tap_count++) {
            const key_state_t one[] = {{.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = tap_count}};

            maps = maps_for(one, ARRAY_SIZE(one));
            CHECK(render_maps(&frame, &profile, &maps));
            CHECK(frame.painted[2] && frame.colors[2].r == case_colors[tap_count >= 5u ? CASE_TAP_5 : CASE_TAP_2 + tap_count - 2u].h);
        }
        // No tap branch pending: the rows for any tap branch stay dark; the
        // 0xff row lights for the tier.
        maps = maps_for(hold_only, ARRAY_SIZE(hold_only));
        CHECK(render_maps(&frame, &profile, &maps));
        CHECK(!frame.painted[2] && !frame.painted[3]);
        CHECK(frame.painted[4] && frame.colors[4].r == 161u);
    }
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
}

// A case whose colour has saturation and value 0 is passthrough: it paints
// nothing, nor do its black rows, while a coloured row for it still paints
// and the other cases still light. Value 0 with saturation is black. The
// newest key's passthrough case still decides its area, as its flash's dark
// phase does (D-F74).
static void test_a_passthrough_case_paints_nothing(void) {
    static const live_row_t rows[] = {
        {.selector = NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_HOLD, .color = {0u, 0u, 0u}, .led_mask = 1u << 0u},
        {.selector = NOAH_PROFILE_RGB_V1_KEY_SEMANTIC_HOLD, .color = {121u, 122u, 123u}, .led_mask = 1u << 3u},
        {.selector = NOAH_PROFILE_RGB_V1_SELECTOR_ALL, .color = {0u, 0u, 0u}, .led_mask = 1u << 1u},
    };
    static const key_state_t scene[] = {
        {.key = 1u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING},
        {.key = 6u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING, .tap_count = 2u},
        {.key = 5u, .semantic = KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY},
    };
    static const key_state_t hold_newest[] = {
        {.key = 2u, .semantic = KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED},
        {.key = 1u, .semantic = KEY_FEEDBACK_SEMANTIC_HOLD_PENDING},
    };
    hsv_t             colors[CASE_COUNT];
    case_localities_t localities = every_case(RGB_KEY_HALF);

    memcpy(colors, case_colors, sizeof(colors));
    colors[CASE_HOLD]      = (hsv_t){90u, 0u, 0u};
    colors[CASE_LONG_HOLD] = (hsv_t){7u, 255u, 0u};
    localities.of[CASE_HOLD]  = RGB_BOTH_HALVES;
    localities.of[CASE_TAP_2] = RGB_KEYS_ONLY;
    noah_active_profile_rgb_runtime_init(&live_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&live_runtime));
    {
        noah_active_profile_rgb_frame_t profile  = live_profile_coloured(colors, localities, rows, ARRAY_SIZE(rows));
        feedback_maps_t                 maps     = maps_for(scene, ARRAY_SIZE(scene));
        expected_frame_t                expected = {0};
        rgb_runtime_frame_t             frame;

        // The hold paints neither both halves nor its black row on LED 0; its
        // coloured row paints LED 3. The long hold paints its half black, the
        // tap branch its key, and the all row the long hold's black.
        expect_leds(&expected, RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT, colors[CASE_LONG_HOLD]);
        expect_leds(&expected, 6u, 7u, colors[CASE_TAP_2]);
        expect_leds(&expected, 3u, 4u, (hsv_t){121u, 122u, 123u});
        expect_leds(&expected, 1u, 2u, colors[CASE_LONG_HOLD]);
        CHECK(render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
    }
    {
        noah_active_profile_rgb_frame_t profile  = live_profile_coloured(colors, every_case(RGB_BOTH_HALVES), NULL, 0u);
        feedback_maps_t                 maps     = maps_for(hold_newest, ARRAY_SIZE(hold_newest));
        expected_frame_t                expected = {0};
        rgb_runtime_frame_t             frame;

        // The newest key's hold decides both halves and paints nothing there,
        // rather than the older tap committed.
        CHECK(!render_maps(&frame, &profile, &maps));
        check_expected(&frame, &expected);
    }
    noah_active_profile_rgb_runtime_uninstall(&live_runtime);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    test_compiled_and_live_key_rendering(argv[1]);
    test_tap_commit_policy_fails_closed_on_stale_read(argv[1]);
    test_one_locality_paints_as_before();
    test_a_narrower_case_paints_over_a_wider_one();
    test_the_newest_key_decides_a_shared_area();
    test_each_tap_branch_is_a_case();
    test_each_case_beside_another();
    test_an_owner_showing_no_case_keeps_its_area_dark();
    test_an_all_row_follows_the_newest_tap_branch();
    test_an_any_tap_branch_row_follows_the_newest_tap_branch();
    test_nothing_showing_paints_nothing();
    test_a_passthrough_case_paints_nothing();
    puts("rgb active-profile behaviour-feedback host tests passed");
    return 0;
}
