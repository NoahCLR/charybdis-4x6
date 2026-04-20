#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/noah_runtime.h"

void           eeconfig_init_user(void);
bool           get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record);
bool           pre_process_record_user(uint16_t keycode, keyrecord_t *record);
bool           process_record_user(uint16_t keycode, keyrecord_t *record);
void           post_process_record_user(uint16_t keycode, keyrecord_t *record);
void           matrix_scan_user(void);
void           housekeeping_task_user(void);
void           keyboard_post_init_user(void);
layer_state_t  layer_state_set_user(layer_state_t state);
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report);
void           pointing_device_init_user(void);
bool           is_mouse_record_user(uint16_t keycode, keyrecord_t *record);
bool           rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max);

layer_state_t layer_state;

typedef struct {
    unsigned eeconfig_calls;
    unsigned hold_calls;
    unsigned pre_process_calls;
    unsigned process_calls;
    unsigned finalize_calls;
    unsigned post_process_calls;
    unsigned scan_calls;
    unsigned housekeeping_calls;
    unsigned post_init_calls;
    unsigned layer_state_calls;
    unsigned pointing_task_calls;
    unsigned pointing_init_calls;
    unsigned mouse_record_calls;
    unsigned rgb_calls;

    uint16_t       hold_keycode;
    keyrecord_t   *hold_record;
    bool           hold_return_value;
    uint16_t       pre_process_keycode;
    keyrecord_t   *pre_process_record;
    bool           pre_process_return_value;
    uint16_t       process_keycode;
    keyrecord_t   *process_record;
    bool           process_return_value;
    uint16_t       finalize_keycode;
    keyrecord_t   *finalize_record;
    bool           finalize_keep_processing;
    uint16_t       post_process_keycode;
    keyrecord_t   *post_process_record;
    layer_state_t  layer_state_input;
    layer_state_t  layer_state_return_value;
    report_mouse_t pointing_task_input;
    report_mouse_t pointing_task_return_value;
    uint16_t       mouse_record_keycode;
    keyrecord_t   *mouse_record_record;
    bool           mouse_record_return_value;
    uint8_t        rgb_led_min;
    uint8_t        rgb_led_max;
    bool           rgb_return_value;
} noah_hook_stub_state_t;

static noah_hook_stub_state_t noah_hook_stub_state;

#ifdef HOOK_CHAINING_TEST_STRONG_OVERRIDE
typedef enum {
    HOOK_PROCESS_CHAIN_AND_NARROW = 0,
    HOOK_PROCESS_CHAIN_PASSTHROUGH_FALSE,
} hook_process_chain_style_t;

typedef struct {
    unsigned                   eeconfig_calls;
    unsigned                   hold_calls;
    unsigned                   pre_process_calls;
    unsigned                   process_calls;
    unsigned                   post_process_calls;
    unsigned                   scan_calls;
    unsigned                   housekeeping_calls;
    unsigned                   post_init_calls;
    unsigned                   layer_state_calls;
    unsigned                   pointing_task_calls;
    unsigned                   pointing_init_calls;
    unsigned                   mouse_record_calls;
    unsigned                   rgb_calls;
    bool                       hold_force_true;
    bool                       pre_process_keep_processing;
    bool                       process_keep_processing;
    hook_process_chain_style_t process_chain_style;
    layer_state_t              layer_state_extra_bits;
    int8_t                     pointing_task_x_delta;
    int8_t                     pointing_task_y_delta;
    uint8_t                    pointing_task_extra_buttons;
    bool                       mouse_record_force_true;
    bool                       rgb_force_true;
} hook_override_state_t;

static hook_override_state_t hook_override_state;
#endif

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

static bool report_mouse_equal(report_mouse_t lhs, report_mouse_t rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.h == rhs.h && lhs.v == rhs.v && lhs.buttons == rhs.buttons;
}

static void test_reset(void) {
    memset(&noah_hook_stub_state, 0, sizeof(noah_hook_stub_state));
#ifdef HOOK_CHAINING_TEST_STRONG_OVERRIDE
    memset(&hook_override_state, 0, sizeof(hook_override_state));
#endif
}

void noah_eeconfig_init_user(void) {
    noah_hook_stub_state.eeconfig_calls++;
}

bool noah_get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    noah_hook_stub_state.hold_calls++;
    noah_hook_stub_state.hold_keycode = keycode;
    noah_hook_stub_state.hold_record  = record;
    return noah_hook_stub_state.hold_return_value;
}

bool noah_pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    noah_hook_stub_state.pre_process_calls++;
    noah_hook_stub_state.pre_process_keycode = keycode;
    noah_hook_stub_state.pre_process_record  = record;
    return noah_hook_stub_state.pre_process_return_value;
}

bool noah_process_record_user(uint16_t keycode, keyrecord_t *record) {
    noah_hook_stub_state.process_calls++;
    noah_hook_stub_state.process_keycode = keycode;
    noah_hook_stub_state.process_record  = record;
    return noah_hook_stub_state.process_return_value;
}

void noah_post_process_record_user(uint16_t keycode, keyrecord_t *record) {
    noah_hook_stub_state.post_process_calls++;
    noah_hook_stub_state.post_process_keycode = keycode;
    noah_hook_stub_state.post_process_record  = record;
    noah_process_record_user_finalize(keycode, record, true);
}

void noah_process_record_user_finalize(uint16_t keycode, keyrecord_t *record, bool keep_processing) {
    noah_hook_stub_state.finalize_calls++;
    noah_hook_stub_state.finalize_keycode         = keycode;
    noah_hook_stub_state.finalize_record          = record;
    noah_hook_stub_state.finalize_keep_processing = keep_processing;
}

void noah_matrix_scan_user(void) {
    noah_hook_stub_state.scan_calls++;
}

void noah_housekeeping_task_user(void) {
    noah_hook_stub_state.housekeeping_calls++;
}

void noah_keyboard_post_init_user(void) {
    noah_hook_stub_state.post_init_calls++;
}

layer_state_t noah_layer_state_set_user(layer_state_t state) {
    noah_hook_stub_state.layer_state_calls++;
    noah_hook_stub_state.layer_state_input = state;
    return noah_hook_stub_state.layer_state_return_value;
}

report_mouse_t noah_pointing_device_task_user(report_mouse_t mouse_report) {
    noah_hook_stub_state.pointing_task_calls++;
    noah_hook_stub_state.pointing_task_input = mouse_report;
    return noah_hook_stub_state.pointing_task_return_value;
}

void noah_pointing_device_init_user(void) {
    noah_hook_stub_state.pointing_init_calls++;
}

bool noah_is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    noah_hook_stub_state.mouse_record_calls++;
    noah_hook_stub_state.mouse_record_keycode = keycode;
    noah_hook_stub_state.mouse_record_record  = record;
    return noah_hook_stub_state.mouse_record_return_value;
}

bool noah_rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    noah_hook_stub_state.rgb_calls++;
    noah_hook_stub_state.rgb_led_min = led_min;
    noah_hook_stub_state.rgb_led_max = led_max;
    return noah_hook_stub_state.rgb_return_value;
}

#ifdef HOOK_CHAINING_TEST_STRONG_OVERRIDE
void eeconfig_init_user(void) {
    hook_override_state.eeconfig_calls++;
    noah_eeconfig_init_user();
}

bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    hook_override_state.hold_calls++;
    return noah_get_hold_on_other_key_press(keycode, record) || hook_override_state.hold_force_true;
}

bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    hook_override_state.pre_process_calls++;
    return noah_pre_process_record_user(keycode, record) && hook_override_state.pre_process_keep_processing;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    bool shared_keep_processing;
    bool keep_processing;

    hook_override_state.process_calls++;
    shared_keep_processing = noah_process_record_user(keycode, record);

    if (hook_override_state.process_chain_style == HOOK_PROCESS_CHAIN_PASSTHROUGH_FALSE) {
        if (!shared_keep_processing) {
            noah_process_record_user_finalize(keycode, record, false);
            return false;
        }

        if (!hook_override_state.process_keep_processing) {
            noah_process_record_user_finalize(keycode, record, false);
            return false;
        }

        return true;
    }

    keep_processing = shared_keep_processing && hook_override_state.process_keep_processing;
    if (!keep_processing) {
        noah_process_record_user_finalize(keycode, record, false);
    }

    return keep_processing;
}

void post_process_record_user(uint16_t keycode, keyrecord_t *record) {
    hook_override_state.post_process_calls++;
    noah_post_process_record_user(keycode, record);
}

void matrix_scan_user(void) {
    hook_override_state.scan_calls++;
    noah_matrix_scan_user();
}

void housekeeping_task_user(void) {
    hook_override_state.housekeeping_calls++;
    noah_housekeeping_task_user();
}

void keyboard_post_init_user(void) {
    hook_override_state.post_init_calls++;
    noah_keyboard_post_init_user();
}

layer_state_t layer_state_set_user(layer_state_t state) {
    hook_override_state.layer_state_calls++;
    return noah_layer_state_set_user(state) | hook_override_state.layer_state_extra_bits;
}

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    hook_override_state.pointing_task_calls++;
    report_mouse_t chained = noah_pointing_device_task_user(mouse_report);
    chained.x += hook_override_state.pointing_task_x_delta;
    chained.y += hook_override_state.pointing_task_y_delta;
    chained.buttons |= hook_override_state.pointing_task_extra_buttons;
    return chained;
}

void pointing_device_init_user(void) {
    hook_override_state.pointing_init_calls++;
    noah_pointing_device_init_user();
}

bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    hook_override_state.mouse_record_calls++;
    return noah_is_mouse_record_user(keycode, record) || hook_override_state.mouse_record_force_true;
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    hook_override_state.rgb_calls++;
    return noah_rgb_matrix_indicators_advanced_user(led_min, led_max) || hook_override_state.rgb_force_true;
}
#endif

static keyrecord_t test_record(uint8_t row, uint8_t col, bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .key     = {.row = row, .col = col},
                .pressed = pressed,
            },
    };
}

#ifndef HOOK_CHAINING_TEST_STRONG_OVERRIDE
static void test_weak_defaults_delegate_to_noah_helpers(void) {
    test_reset();

    keyrecord_t    record       = test_record(1, 2, true);
    layer_state_t  input_state  = 0x00000012u;
    report_mouse_t input_report = {.x = 1, .y = -2, .h = 3, .v = -4, .buttons = 5};

    noah_hook_stub_state.hold_return_value          = true;
    noah_hook_stub_state.pre_process_return_value   = true;
    noah_hook_stub_state.process_return_value       = false;
    noah_hook_stub_state.layer_state_return_value   = 0x00000034u;
    noah_hook_stub_state.pointing_task_return_value = (report_mouse_t){.x = -5, .y = 6, .h = -7, .v = 8, .buttons = 9};
    noah_hook_stub_state.mouse_record_return_value  = true;
    noah_hook_stub_state.rgb_return_value           = false;

    eeconfig_init_user();
    CHECK(noah_hook_stub_state.eeconfig_calls == 1);

    CHECK(get_hold_on_other_key_press(0x1234u, &record));
    CHECK(noah_hook_stub_state.hold_calls == 1);
    CHECK(noah_hook_stub_state.hold_keycode == 0x1234u);
    CHECK(noah_hook_stub_state.hold_record == &record);

    CHECK(pre_process_record_user(0x2222u, &record));
    CHECK(noah_hook_stub_state.pre_process_calls == 1);
    CHECK(noah_hook_stub_state.pre_process_keycode == 0x2222u);
    CHECK(noah_hook_stub_state.pre_process_record == &record);

    CHECK(!process_record_user(0x2345u, &record));
    CHECK(noah_hook_stub_state.process_calls == 1);
    CHECK(noah_hook_stub_state.process_keycode == 0x2345u);
    CHECK(noah_hook_stub_state.process_record == &record);
    CHECK(noah_hook_stub_state.finalize_calls == 1);
    CHECK(noah_hook_stub_state.finalize_keycode == 0x2345u);
    CHECK(noah_hook_stub_state.finalize_record == &record);
    CHECK(!noah_hook_stub_state.finalize_keep_processing);

    post_process_record_user(0x2456u, &record);
    CHECK(noah_hook_stub_state.post_process_calls == 1);
    CHECK(noah_hook_stub_state.post_process_keycode == 0x2456u);
    CHECK(noah_hook_stub_state.post_process_record == &record);
    CHECK(noah_hook_stub_state.finalize_calls == 2);
    CHECK(noah_hook_stub_state.finalize_keycode == 0x2456u);
    CHECK(noah_hook_stub_state.finalize_record == &record);
    CHECK(noah_hook_stub_state.finalize_keep_processing);

    matrix_scan_user();
    CHECK(noah_hook_stub_state.scan_calls == 1);

    housekeeping_task_user();
    CHECK(noah_hook_stub_state.housekeeping_calls == 1);

    keyboard_post_init_user();
    CHECK(noah_hook_stub_state.post_init_calls == 1);

    CHECK(layer_state_set_user(input_state) == noah_hook_stub_state.layer_state_return_value);
    CHECK(noah_hook_stub_state.layer_state_calls == 1);
    CHECK(noah_hook_stub_state.layer_state_input == input_state);

    report_mouse_t chained_report = pointing_device_task_user(input_report);
    CHECK(noah_hook_stub_state.pointing_task_calls == 1);
    CHECK(report_mouse_equal(noah_hook_stub_state.pointing_task_input, input_report));
    CHECK(report_mouse_equal(chained_report, noah_hook_stub_state.pointing_task_return_value));

    pointing_device_init_user();
    CHECK(noah_hook_stub_state.pointing_init_calls == 1);

    CHECK(is_mouse_record_user(0x3456u, &record));
    CHECK(noah_hook_stub_state.mouse_record_calls == 1);
    CHECK(noah_hook_stub_state.mouse_record_keycode == 0x3456u);
    CHECK(noah_hook_stub_state.mouse_record_record == &record);

    CHECK(!rgb_matrix_indicators_advanced_user(3, 9));
    CHECK(noah_hook_stub_state.rgb_calls == 1);
    CHECK(noah_hook_stub_state.rgb_led_min == 3);
    CHECK(noah_hook_stub_state.rgb_led_max == 9);
}
#else
static void test_strong_overrides_can_chain_to_noah_helpers(void) {
    test_reset();

    keyrecord_t    record       = test_record(4, 5, false);
    layer_state_t  input_state  = 0x00000021u;
    report_mouse_t input_report = {.x = 10, .y = 11, .h = 12, .v = 13, .buttons = 14};

    noah_hook_stub_state.hold_return_value          = false;
    noah_hook_stub_state.pre_process_return_value   = true;
    noah_hook_stub_state.process_return_value       = true;
    noah_hook_stub_state.layer_state_return_value   = 0x00000040u;
    noah_hook_stub_state.pointing_task_return_value = (report_mouse_t){.x = 1, .y = 2, .h = 3, .v = 4, .buttons = 5};
    noah_hook_stub_state.mouse_record_return_value  = false;
    noah_hook_stub_state.rgb_return_value           = false;

    hook_override_state.hold_force_true             = true;
    hook_override_state.pre_process_keep_processing = false;
    hook_override_state.process_keep_processing     = false;
    hook_override_state.layer_state_extra_bits      = 0x00000080u;
    hook_override_state.pointing_task_x_delta       = 6;
    hook_override_state.pointing_task_y_delta       = -3;
    hook_override_state.pointing_task_extra_buttons = 0x20u;
    hook_override_state.mouse_record_force_true     = true;
    hook_override_state.rgb_force_true              = true;

    eeconfig_init_user();
    CHECK(hook_override_state.eeconfig_calls == 1);
    CHECK(noah_hook_stub_state.eeconfig_calls == 1);

    CHECK(get_hold_on_other_key_press(0x4567u, &record));
    CHECK(hook_override_state.hold_calls == 1);
    CHECK(noah_hook_stub_state.hold_calls == 1);
    CHECK(noah_hook_stub_state.hold_keycode == 0x4567u);
    CHECK(noah_hook_stub_state.hold_record == &record);

    CHECK(!pre_process_record_user(0x5555u, &record));
    CHECK(hook_override_state.pre_process_calls == 1);
    CHECK(noah_hook_stub_state.pre_process_calls == 1);
    CHECK(noah_hook_stub_state.pre_process_keycode == 0x5555u);
    CHECK(noah_hook_stub_state.pre_process_record == &record);

    CHECK(!process_record_user(0x5678u, &record));
    CHECK(hook_override_state.process_calls == 1);
    CHECK(noah_hook_stub_state.process_calls == 1);
    CHECK(noah_hook_stub_state.process_keycode == 0x5678u);
    CHECK(noah_hook_stub_state.process_record == &record);
    CHECK(noah_hook_stub_state.finalize_calls == 1);
    CHECK(noah_hook_stub_state.finalize_keycode == 0x5678u);
    CHECK(noah_hook_stub_state.finalize_record == &record);
    CHECK(!noah_hook_stub_state.finalize_keep_processing);

    post_process_record_user(0x5789u, &record);
    CHECK(hook_override_state.post_process_calls == 1);
    CHECK(noah_hook_stub_state.post_process_calls == 1);
    CHECK(noah_hook_stub_state.post_process_keycode == 0x5789u);
    CHECK(noah_hook_stub_state.post_process_record == &record);
    CHECK(noah_hook_stub_state.finalize_calls == 2);
    CHECK(noah_hook_stub_state.finalize_keycode == 0x5789u);
    CHECK(noah_hook_stub_state.finalize_record == &record);
    CHECK(noah_hook_stub_state.finalize_keep_processing);

    matrix_scan_user();
    CHECK(hook_override_state.scan_calls == 1);
    CHECK(noah_hook_stub_state.scan_calls == 1);

    housekeeping_task_user();
    CHECK(hook_override_state.housekeeping_calls == 1);
    CHECK(noah_hook_stub_state.housekeeping_calls == 1);

    keyboard_post_init_user();
    CHECK(hook_override_state.post_init_calls == 1);
    CHECK(noah_hook_stub_state.post_init_calls == 1);

    CHECK(layer_state_set_user(input_state) == (noah_hook_stub_state.layer_state_return_value | hook_override_state.layer_state_extra_bits));
    CHECK(hook_override_state.layer_state_calls == 1);
    CHECK(noah_hook_stub_state.layer_state_calls == 1);
    CHECK(noah_hook_stub_state.layer_state_input == input_state);

    report_mouse_t chained_report = pointing_device_task_user(input_report);
    CHECK(hook_override_state.pointing_task_calls == 1);
    CHECK(noah_hook_stub_state.pointing_task_calls == 1);
    CHECK(report_mouse_equal(noah_hook_stub_state.pointing_task_input, input_report));
    CHECK(report_mouse_equal(chained_report, (report_mouse_t){
                                                 .x       = 7,
                                                 .y       = -1,
                                                 .h       = 3,
                                                 .v       = 4,
                                                 .buttons = 0x25,
                                             }));

    pointing_device_init_user();
    CHECK(hook_override_state.pointing_init_calls == 1);
    CHECK(noah_hook_stub_state.pointing_init_calls == 1);

    CHECK(is_mouse_record_user(0x6789u, &record));
    CHECK(hook_override_state.mouse_record_calls == 1);
    CHECK(noah_hook_stub_state.mouse_record_calls == 1);
    CHECK(noah_hook_stub_state.mouse_record_keycode == 0x6789u);
    CHECK(noah_hook_stub_state.mouse_record_record == &record);

    CHECK(rgb_matrix_indicators_advanced_user(2, 7));
    CHECK(hook_override_state.rgb_calls == 1);
    CHECK(noah_hook_stub_state.rgb_calls == 1);
    CHECK(noah_hook_stub_state.rgb_led_min == 2);
    CHECK(noah_hook_stub_state.rgb_led_max == 7);
}

static void test_strong_override_passthrough_false_path_finalizes_without_post(void) {
    test_reset();

    keyrecord_t record = test_record(6, 1, true);

    noah_hook_stub_state.process_return_value   = false;
    hook_override_state.process_keep_processing = true;
    hook_override_state.process_chain_style     = HOOK_PROCESS_CHAIN_PASSTHROUGH_FALSE;

    CHECK(!process_record_user(0x6123u, &record));
    CHECK(hook_override_state.process_calls == 1);
    CHECK(noah_hook_stub_state.process_calls == 1);
    CHECK(noah_hook_stub_state.process_keycode == 0x6123u);
    CHECK(noah_hook_stub_state.process_record == &record);
    CHECK(noah_hook_stub_state.finalize_calls == 1);
    CHECK(noah_hook_stub_state.finalize_keycode == 0x6123u);
    CHECK(noah_hook_stub_state.finalize_record == &record);
    CHECK(!noah_hook_stub_state.finalize_keep_processing);
    CHECK(noah_hook_stub_state.post_process_calls == 0);
}
#endif

int main(void) {
#ifndef HOOK_CHAINING_TEST_STRONG_OVERRIDE
    test_weak_defaults_delegate_to_noah_helpers();
#else
    test_strong_overrides_can_chain_to_noah_helpers();
    test_strong_override_passthrough_false_path_finalizes_without_post();
#endif
    return 0;
}
