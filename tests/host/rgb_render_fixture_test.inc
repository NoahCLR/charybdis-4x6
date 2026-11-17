// The shared drawing cases (tests/fixtures/rgb_render_v1.json), run through
// the whole render path of this harness: the real board's 58 LEDs, both
// halves chunk by chunk as QMK calls them, a lighting profile published as
// the active profile and QMK's own hsv_to_rgb. run_rgb_layer_render_tests.sh
// turns the JSON into the lines read here and checks its board against the
// keyboard's own tables.
//
// Lines: `limits <stages> <layers> <brightness> <tap branches> <slot mask>`,
// `led <led> <row> <col>` (255 255 for an LED no key carries), `map <layer>
// <row> <col>`, `background <h> <s> <v>`, then per case `case <name>`, `rgb
// <hex>`, `lighting <on> <idle>`, `layers <mask>`, `pointing <slot> <row>
// <col> <link mask>`, `combo <runs the mode> <count> <row> <col>...`,
// `behaviour <case> <tap count> <row> <col>`, `replacement <case> <count>
// <row> <col>...`, `expect <led> <r> <g> <b>` and `end`.

#include "users/noah/lib/profile/schema/profile_validator_v1.h"

#if RGB_MATRIX_LED_COUNT != 58 || RGB_LEFT_LED_COUNT != 29 || MATRIX_ROWS != 10 || MATRIX_COLS != 6 || !defined(RGB_MATRIX_SPLIT)
#    error "The drawing cases are the real board's: 58 LEDs, 29 a half, a 10 x 6 split matrix"
#endif

// A case's lighting is at most the largest lighting domain, 4,661 bytes.
#define FIXTURE_RGB_MAX_SIZE 4661u
#define FIXTURE_HEX_FORMAT "%9322s"

static uint32_t fixture_lighting_timeout_ms;
static uint32_t fixture_input_idle_ms;

uint32_t noah_setting(uint8_t id, uint32_t fallback) {
    return id == NOAH_SETTING_RGB_TIMEOUT ? fixture_lighting_timeout_ms : fallback;
}

uint32_t last_input_activity_elapsed(void) {
    return fixture_input_idle_ms;
}

typedef struct {
    noah_profile_rgb_v1_limits_t limits;
    uint8_t                      matrix_co[MATRIX_ROWS][MATRIX_COLS];
    bool                         mapped[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
    rgb_t                        background;
} fixture_board_t;

typedef struct {
    char                 name[96];
    uint8_t              payload[FIXTURE_RGB_MAX_SIZE];
    size_t               length;
    bool                 lighting_on;
    bool                 idle;
    layer_state_t        layers;
    pointing_mode_mask_t pointing_mode;
    keypos_t             pointing_key;
    uint16_t             link;
    bool                 combo_runs_mode;
    uint8_t              combo_keys[KEY_ORIGIN_BITMAP_SIZE];
    key_feedback_semantic_t behaviour;
    uint8_t              tap_count;
    keypos_t             behaviour_key;
    uint8_t              replacement;
    uint8_t              replacement_keys[KEY_ORIGIN_BITMAP_SIZE];
    rgb_t                expected[RGB_MATRIX_LED_COUNT];
    bool                 expected_set[RGB_MATRIX_LED_COUNT];
} fixture_case_t;


static noah_active_profile_rgb_runtime_t fixture_runtime;
static uint32_t                          fixture_generation;

static uint8_t fixture_hex_nibble(char value) {
    if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if (value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    test_fail("fixture hex", __FILE__, __LINE__);
    return 0u;
}

// Row 255 is no key.
static keypos_t fixture_keypos(unsigned row, unsigned col) {
    return (keypos_t){.row = (uint8_t)row, .col = (uint8_t)col};
}

static bool fixture_keypos_known(keypos_t key) {
    return key.row < MATRIX_ROWS && key.col < MATRIX_COLS;
}

static void fixture_read_keys(FILE *file, uint8_t *bitmap) {
    unsigned count, row, col;

    key_origin_bitmap_clear(bitmap);
    CHECK(fscanf(file, "%u", &count) == 1);
    for (unsigned index = 0u; index < count; index++) {
        CHECK(fscanf(file, "%u %u", &row, &col) == 2 && row < MATRIX_ROWS && col < MATRIX_COLS);
        key_origin_bitmap_add_keypos(bitmap, (keypos_t){.row = (uint8_t)row, .col = (uint8_t)col});
    }
}

static key_feedback_semantic_t fixture_behaviour(const char *name) {
    // A steady state of each case: flashing is timing, which a still frame
    // does not hold.
    if (strcmp(name, "tapBranch") == 0) return KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING;
    if (strcmp(name, "tapCommitted") == 0) return KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED;
    if (strcmp(name, "hold") == 0) return KEY_FEEDBACK_SEMANTIC_HOLD_PENDING;
    if (strcmp(name, "longHold") == 0) return KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY;
    test_fail("fixture behaviour case", __FILE__, __LINE__);
    return KEY_FEEDBACK_SEMANTIC_NONE;
}

static uint8_t fixture_replacement(const char *name) {
    if (strcmp(name, "textReplaced") == 0) return NOAH_AUTOCORRECT_FEEDBACK_TEXT;
    if (strcmp(name, "macroReplaced") == 0) return NOAH_AUTOCORRECT_FEEDBACK_MACRO;
    if (strcmp(name, "undone") == 0) return NOAH_AUTOCORRECT_FEEDBACK_UNDO;
    if (strcmp(name, "leading") == 0) return NOAH_AUTOCORRECT_FEEDBACK_LEADING;
    test_fail("fixture replacement case", __FILE__, __LINE__);
    return NOAH_AUTOCORRECT_FEEDBACK_NONE;
}

// The board, keymap and background lines before the first case.
static void fixture_read_board(FILE *file, fixture_board_t *board, char *keyword) {
    unsigned stages, layers, brightness, branches, led, row, col, layer, h, s, v;
    unsigned long slots;

    memset(board, 0, sizeof(*board));
    memset(board->matrix_co, NO_LED, sizeof(board->matrix_co));
    board->limits = noah_profile_rgb_v1_default_limits();
    while (fscanf(file, "%31s", keyword) == 1 && strcmp(keyword, "case") != 0) {
        if (strcmp(keyword, "limits") == 0) {
            CHECK(fscanf(file, "%u %u %u %u %lu", &stages, &layers, &brightness, &branches, &slots) == 5);
            board->limits.compiled_stage_mask          = (uint16_t)stages;
            board->limits.logical_layer_count          = (uint8_t)layers;
            board->limits.maximum_brightness           = (uint8_t)brightness;
            board->limits.tap_branch_color_count       = (uint8_t)branches;
            board->limits.supported_pointing_mode_mask = (uint32_t)slots;
        } else if (strcmp(keyword, "led") == 0) {
            CHECK(fscanf(file, "%u %u %u", &led, &row, &col) == 3 && led < RGB_MATRIX_LED_COUNT);
            if (row != 255u) {
                CHECK(row < MATRIX_ROWS && col < MATRIX_COLS);
                board->matrix_co[row][col] = (uint8_t)led;
            }
        } else if (strcmp(keyword, "map") == 0) {
            CHECK(fscanf(file, "%u %u %u", &layer, &row, &col) == 3 && layer < LAYER_COUNT && row < MATRIX_ROWS && col < MATRIX_COLS);
            board->mapped[layer][row][col] = true;
        } else if (strcmp(keyword, "background") == 0) {
            CHECK(fscanf(file, "%u %u %u", &h, &s, &v) == 3);
            board->background = hsv_to_rgb((hsv_t){.h = (uint8_t)h, .s = (uint8_t)s, .v = (uint8_t)v});
        } else {
            test_fail(keyword, __FILE__, __LINE__);
        }
    }
}

// One case, after its `case` keyword; false at the end of the file.
static bool fixture_read_case(FILE *file, fixture_case_t *drawn) {
    static char hex[FIXTURE_RGB_MAX_SIZE * 2u + 1u];
    char        keyword[32], word[32];
    unsigned    a, b, c, d;

    memset(drawn, 0, sizeof(*drawn));
    drawn->pointing_key  = fixture_keypos(255u, 0u);
    drawn->behaviour_key = fixture_keypos(255u, 0u);
    if (fscanf(file, "%95s", drawn->name) != 1) {
        return false;
    }
    while (fscanf(file, "%31s", keyword) == 1 && strcmp(keyword, "end") != 0) {
        if (strcmp(keyword, "rgb") == 0) {
            CHECK(fscanf(file, FIXTURE_HEX_FORMAT, hex) == 1);
            drawn->length = strlen(hex) / 2u;
            CHECK(strlen(hex) % 2u == 0u && drawn->length <= sizeof(drawn->payload));
            for (size_t index = 0u; index < drawn->length; index++) {
                drawn->payload[index] = (uint8_t)(fixture_hex_nibble(hex[2u * index]) << 4u | fixture_hex_nibble(hex[2u * index + 1u]));
            }
        } else if (strcmp(keyword, "lighting") == 0) {
            CHECK(fscanf(file, "%u %u", &a, &b) == 2);
            drawn->lighting_on = a != 0u;
            drawn->idle        = b != 0u;
        } else if (strcmp(keyword, "layers") == 0) {
            CHECK(fscanf(file, "%u", &a) == 1);
            drawn->layers = (layer_state_t)a;
        } else if (strcmp(keyword, "pointing") == 0) {
            CHECK(fscanf(file, "%u %u %u %u", &a, &b, &c, &d) == 4 && a < POINTING_MODE_COUNT);
            drawn->pointing_mode = pointing_mode_mask_from_id((pointing_mode_id_t)a);
            drawn->pointing_key  = fixture_keypos(b, c);
            drawn->link          = (uint16_t)d;
        } else if (strcmp(keyword, "combo") == 0) {
            CHECK(fscanf(file, "%u", &a) == 1);
            drawn->combo_runs_mode = a != 0u;
            fixture_read_keys(file, drawn->combo_keys);
        } else if (strcmp(keyword, "behaviour") == 0) {
            CHECK(fscanf(file, "%31s %u %u %u", word, &a, &b, &c) == 4 && b < MATRIX_ROWS && c < MATRIX_COLS);
            drawn->behaviour     = fixture_behaviour(word);
            drawn->tap_count     = (uint8_t)a;
            drawn->behaviour_key = fixture_keypos(b, c);
        } else if (strcmp(keyword, "replacement") == 0) {
            CHECK(fscanf(file, "%31s", word) == 1);
            drawn->replacement = fixture_replacement(word);
            fixture_read_keys(file, drawn->replacement_keys);
        } else if (strcmp(keyword, "expect") == 0) {
            CHECK(fscanf(file, "%u %u %u %u", &a, &b, &c, &d) == 4 && a < RGB_MATRIX_LED_COUNT);
            drawn->expected[a]     = (rgb_t){.r = (uint8_t)b, .g = (uint8_t)c, .b = (uint8_t)d};
            drawn->expected_set[a] = true;
        } else {
            test_fail(keyword, __FILE__, __LINE__);
        }
    }
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        CHECK(drawn->expected_set[led]);
    }
    return true;
}

// Publishes a case's lighting as the active profile, as an activation does.
static void fixture_publish(const fixture_board_t *board, const fixture_case_t *drawn) {
    noah_active_profile_snapshot_t snapshot = {0};
    noah_profile_rgb_v1_error_t    error;

    fixture_generation++;
    snapshot.identity            = (noah_active_profile_identity_t){.generation = fixture_generation, .payload_crc32 = fixture_generation, .kind = NOAH_ACTIVE_PROFILE_KIND_VALIDATED_PROFILE};
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB;
    if (noah_profile_rgb_v1_decode(drawn->payload, drawn->length, &board->limits, &snapshot.profile.rgb, &error) != NOAH_PROFILE_RGB_V1_OK) {
        fprintf(stderr, "%s: the lighting profile does not decode (offset %zu)\n", drawn->name, error.offset);
        exit(1);
    }
    noah_active_profile_rgb_runtime_invalidate(&fixture_runtime, fixture_generation, (noah_active_profile_identity_t){0}, snapshot.identity, &snapshot);
}

// The case's state, set where each stage reads it on the master half.
static void fixture_set_state(const fixture_board_t *board, const fixture_case_t *drawn) {
    test_reset();
    memcpy(g_led_config.matrix_co, board->matrix_co, sizeof(board->matrix_co));
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
                test_keymap[layer][row][col] = board->mapped[layer][row][col] ? KC_A : KC_TRNS;
            }
        }
    }
    // No case shows the auto-mouse fade: its layer is one no case turns on.
    fake_auto_mouse_layer   = LAYER_COUNT - 1u;
    fake_auto_mouse_active  = false;
    fake_auto_mouse_enabled = false;
    layer_state             = drawn->layers;

    fake_pd_active_mode = drawn->pointing_mode;
    fake_linked_mode    = drawn->pointing_mode;
    fake_linked_layers  = drawn->link;
    if (drawn->pointing_mode && fixture_keypos_known(drawn->pointing_key)) {
        fake_pd_owner_sides = split_side_mask_add_keypos(SPLIT_SIDE_MASK_NONE, drawn->pointing_key);
        key_origin_bitmap_fill_single(fake_pd_owner_bitmap, drawn->pointing_key);
    }

    key_origin_bitmap_copy(drawn->combo_runs_mode ? fake_combo_underlay_bitmap : fake_combo_overlay_bitmap, drawn->combo_keys);

    if (drawn->behaviour != KEY_FEEDBACK_SEMANTIC_NONE) {
        test_local_feedback_semantic_add(drawn->behaviour_key.row, drawn->behaviour_key.col, drawn->behaviour);
        if (drawn->behaviour == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING) {
            test_local_feedback_tap_branch_add(drawn->behaviour_key.row, drawn->behaviour_key.col, drawn->tap_count);
        }
    }

    fake_autocorrect_feedback.kind = drawn->replacement;
    memcpy(fake_autocorrect_feedback.keys, drawn->replacement_keys, sizeof(fake_autocorrect_feedback.keys));

    fixture_lighting_timeout_ms = 60000u;
    fixture_input_idle_ms       = drawn->idle ? fixture_lighting_timeout_ms + 1u : 0u;
}

// One frame: QMK's effect (the background) on every LED, then each half's
// chunks as rgb_matrix_task calls the hook, unless QMK's switch is off, when
// its effect is none and it calls no hook.
static void fixture_draw(const fixture_board_t *board, const fixture_case_t *drawn) {
    for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
        led_output[led] = drawn->lighting_on ? board->background : (rgb_t){0};
    }
    if (!drawn->lighting_on) {
        return;
    }
    for (uint8_t half = 0u; half < 2u; half++) {
        fake_is_left = half == 0u;
        for (uint8_t iter = 0u; (uint16_t)iter * RGB_MATRIX_LED_PROCESS_LIMIT < RGB_MATRIX_LED_COUNT; iter++) {
            struct rgb_matrix_limits_t limits = rgb_matrix_get_limits(iter);
            (void)noah_rgb_matrix_indicators_advanced_user(limits.led_min_index, limits.led_max_index);
        }
    }
    fake_is_left = true;
}

static int rgb_render_fixture_main(int argc, char **argv) {
    static fixture_board_t board;
    static fixture_case_t  drawn;
    char                   keyword[32];
    unsigned               cases = 0u, failed = 0u;
    FILE                  *file;

    CHECK(argc == 2);
    file = fopen(argv[1], "r");
    CHECK(file != NULL);
    fixture_read_board(file, &board, keyword);
    CHECK(strcmp(keyword, "case") == 0);
    noah_active_profile_rgb_runtime_init(&fixture_runtime);
    CHECK(noah_active_profile_rgb_runtime_install(&fixture_runtime));
    do {
        bool same = true;

        if (!fixture_read_case(file, &drawn)) {
            break;
        }
        cases++;
        fixture_publish(&board, &drawn);
        fixture_set_state(&board, &drawn);
        fixture_draw(&board, &drawn);
        for (uint8_t led = 0u; led < RGB_MATRIX_LED_COUNT; led++) {
            rgb_t got = led_output[led], want = drawn.expected[led];

            if (got.r != want.r || got.g != want.g || got.b != want.b) {
                fprintf(stderr, "%s: LED %u is (%u, %u, %u), the case expects (%u, %u, %u)\n", drawn.name, (unsigned)led, (unsigned)got.r, (unsigned)got.g, (unsigned)got.b, (unsigned)want.r, (unsigned)want.g, (unsigned)want.b);
                same = false;
            }
        }
        failed += same ? 0u : 1u;
    } while (fscanf(file, "%31s", keyword) == 1 && strcmp(keyword, "case") == 0);
    fclose(file);
    noah_active_profile_rgb_runtime_uninstall(&fixture_runtime);
    CHECK(cases > 0u);
    if (failed) {
        fprintf(stderr, "rgb_render_v1.json: %u of %u cases drew differently\n", failed, cases);
        return 1;
    }
    printf("rgb_render_v1.json: %u drawing cases passed\n", cases);
    return 0;
}
