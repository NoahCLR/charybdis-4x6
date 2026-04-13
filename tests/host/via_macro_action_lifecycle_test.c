#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "send_string.h"

typedef enum {
    TEST_CALL_NONE = 0,
    TEST_CALL_OWNED_TAP,
    TEST_CALL_OWNED_REGISTER,
    TEST_CALL_OWNED_UNREGISTER,
    TEST_CALL_SEND_CHAR,
    TEST_CALL_WAIT,
} test_call_kind_t;

typedef struct {
    test_call_kind_t kind;
    uint16_t         value;
    uint8_t          interval;
} test_call_t;

#define TEST_MAX_CALLS 32
#define TEST_MACRO_BUFFER_SIZE 64

static uint8_t     macro_buffer[TEST_MACRO_BUFFER_SIZE];
static uint16_t    fake_macro_buffer_size;
static test_call_t test_calls[TEST_MAX_CALLS];
static uint8_t     test_call_count;

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

static void test_log_call(test_call_kind_t kind, uint16_t value, uint8_t interval) {
    CHECK(test_call_count < TEST_MAX_CALLS);
    test_calls[test_call_count++] = (test_call_t){
        .kind     = kind,
        .value    = value,
        .interval = interval,
    };
}

static void test_reset_state(void) {
    memset(macro_buffer, 0, sizeof(macro_buffer));
    memset(test_calls, 0, sizeof(test_calls));
    fake_macro_buffer_size = sizeof(macro_buffer);
    test_call_count        = 0;
}

uint8_t dynamic_keymap_macro_get_count(void) {
    return 2;
}

uint16_t dynamic_keymap_macro_get_buffer_size(void) {
    return fake_macro_buffer_size;
}

void dynamic_keymap_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    memcpy(data, &macro_buffer[offset], size);
}

bool owned_keycode_tap(uint16_t keycode) {
    test_log_call(TEST_CALL_OWNED_TAP, keycode, 0);
    return true;
}

bool owned_keycode_register(uint16_t keycode) {
    test_log_call(TEST_CALL_OWNED_REGISTER, keycode, 0);
    return true;
}

bool owned_keycode_unregister(uint16_t keycode) {
    test_log_call(TEST_CALL_OWNED_UNREGISTER, keycode, 0);
    return true;
}

void send_char_with_delay(char ascii_code, uint8_t interval) {
    test_log_call(TEST_CALL_SEND_CHAR, (uint8_t)ascii_code, interval);
}

void wait_ms(uint16_t ms) {
    test_log_call(TEST_CALL_WAIT, ms, 0);
}

bool macro_dispatch(uint16_t action) {
    (void)action;
    return false;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    (void)action;
    return NULL;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool pd_mode_handle_keycode_press(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool pd_mode_handle_keycode_release(uint16_t keycode) {
    (void)keycode;
    return false;
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    (void)key_pos;
    (void)layer;
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

bool layer_ownership_toggle_lock_state(uint8_t layer) {
    (void)layer;
    return false;
}

void split_runtime_sync(void) {}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    (void)keycode;
    CHECK(false);
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    (void)keycode;
    CHECK(false);
}

void noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    (void)keycode;
    (void)pressed;
    CHECK(false);
}

void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count) {
    (void)keycode;
    (void)pressed;
    (void)tap_count;
    CHECK(false);
}

void tap_code16(uint16_t keycode) {
    (void)keycode;
    CHECK(false);
}

void register_code16(uint16_t keycode) {
    (void)keycode;
    CHECK(false);
}

void unregister_code16(uint16_t keycode) {
    (void)keycode;
    CHECK(false);
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

static void test_qmk_tap_command_uses_owned_tap(void) {
    test_reset_state();
    macro_buffer[0] = SS_QMK_PREFIX;
    macro_buffer[1] = SS_TAP_CODE;
    macro_buffer[2] = KC_C;

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_TAP);
    CHECK(test_calls[0].value == KC_C);
    CHECK(test_calls[1].kind == TEST_CALL_WAIT);
    CHECK(test_calls[1].value == TAP_CODE_DELAY);
}

static void test_qmk_down_and_up_commands_use_owned_register_and_unregister(void) {
    test_reset_state();
    macro_buffer[0] = SS_QMK_PREFIX;
    macro_buffer[1] = SS_DOWN_CODE;
    macro_buffer[2] = KC_LEFT_SHIFT;
    macro_buffer[3] = SS_QMK_PREFIX;
    macro_buffer[4] = SS_UP_CODE;
    macro_buffer[5] = KC_LEFT_SHIFT;

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 4);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_REGISTER);
    CHECK(test_calls[0].value == KC_LEFT_SHIFT);
    CHECK(test_calls[1].kind == TEST_CALL_WAIT);
    CHECK(test_calls[1].value == TAP_CODE_DELAY);
    CHECK(test_calls[2].kind == TEST_CALL_OWNED_UNREGISTER);
    CHECK(test_calls[2].value == KC_LEFT_SHIFT);
    CHECK(test_calls[3].kind == TEST_CALL_WAIT);
    CHECK(test_calls[3].value == TAP_CODE_DELAY);
}

static void test_delay_command_matches_upstream_parsing(void) {
    test_reset_state();
    macro_buffer[0] = SS_QMK_PREFIX;
    macro_buffer[1] = SS_DELAY_CODE;
    macro_buffer[2] = '2';
    macro_buffer[3] = '5';
    macro_buffer[4] = '0';
    macro_buffer[5] = '|';
    macro_buffer[6] = 'X';

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 3);
    CHECK(test_calls[0].kind == TEST_CALL_WAIT);
    CHECK(test_calls[0].value == 250);
    CHECK(test_calls[1].kind == TEST_CALL_WAIT);
    CHECK(test_calls[1].value == TAP_CODE_DELAY);
    CHECK(test_calls[2].kind == TEST_CALL_SEND_CHAR);
    CHECK(test_calls[2].value == 'X');
    CHECK(test_calls[2].interval == TAP_CODE_DELAY);
}

static void test_plain_text_uses_send_char_with_delay(void) {
    test_reset_state();
    macro_buffer[0] = 'H';
    macro_buffer[1] = 'i';

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_SEND_CHAR);
    CHECK(test_calls[0].value == 'H');
    CHECK(test_calls[0].interval == TAP_CODE_DELAY);
    CHECK(test_calls[1].kind == TEST_CALL_SEND_CHAR);
    CHECK(test_calls[1].value == 'i');
    CHECK(test_calls[1].interval == TAP_CODE_DELAY);
}

static void test_macro_slot_lookup_skips_null_terminated_entries(void) {
    test_reset_state();
    macro_buffer[0] = 'A';
    macro_buffer[1] = 0;
    macro_buffer[2] = SS_QMK_PREFIX;
    macro_buffer[3] = SS_TAP_CODE;
    macro_buffer[4] = KC_V;

    noah_action_tap(QK_MACRO_0 + 1);

    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_TAP);
    CHECK(test_calls[0].value == KC_V);
    CHECK(test_calls[1].kind == TEST_CALL_WAIT);
    CHECK(test_calls[1].value == TAP_CODE_DELAY);
}

static void test_out_of_range_macro_slot_is_ignored(void) {
    test_reset_state();
    macro_buffer[0] = 'A';
    macro_buffer[1] = 0;

    noah_action_tap(QK_MACRO_0 + 2);

    CHECK(test_call_count == 0);
}

static void test_zero_sized_macro_buffer_is_ignored(void) {
    test_reset_state();
    fake_macro_buffer_size = 0;

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 0);
}

static void test_non_terminated_macro_buffer_is_ignored(void) {
    test_reset_state();
    macro_buffer[0]                          = 'A';
    macro_buffer[TEST_MACRO_BUFFER_SIZE - 1] = 'Z';

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 0);
}

static void test_unterminated_delay_command_matches_current_via_behavior(void) {
    test_reset_state();
    fake_macro_buffer_size = 5;
    macro_buffer[0]        = SS_QMK_PREFIX;
    macro_buffer[1]        = SS_DELAY_CODE;
    macro_buffer[2]        = '2';
    macro_buffer[3]        = '5';
    macro_buffer[4]        = 0;

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_WAIT);
    CHECK(test_calls[0].value == 25);
    CHECK(test_calls[1].kind == TEST_CALL_WAIT);
    CHECK(test_calls[1].value == TAP_CODE_DELAY);
}

int main(void) {
    test_qmk_tap_command_uses_owned_tap();
    test_qmk_down_and_up_commands_use_owned_register_and_unregister();
    test_delay_command_matches_upstream_parsing();
    test_plain_text_uses_send_char_with_delay();
    test_macro_slot_lookup_skips_null_terminated_entries();
    test_out_of_range_macro_slot_is_ignored();
    test_zero_sized_macro_buffer_is_ignored();
    test_non_terminated_macro_buffer_is_ignored();
    test_unterminated_delay_command_matches_current_via_behavior();

    puts("via macro action_lifecycle host tests passed");
    return 0;
}
