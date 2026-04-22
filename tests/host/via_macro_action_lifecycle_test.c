#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/macro/macro_payload.h"
#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/lib/macro/via_macro_provider.h"
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

#define TEST_KC_8 0x0025u
#define TEST_MAX_CALLS 512
#define TEST_MACRO_BUFFER_SIZE 512

static const char *const test_long_delay_heavy_payload = "h{829}e{627}y{665} {249}h{158}a{167}l{424}o{386} {448}h{144}o{111}e{103} {118}i{123}s{118} {125}h{132}e{134}t{158} {133}m{118}e{493}t{156} {503}y{503}u{10}o{695} {382}h{113}e{212}b{149}b{83}e{155}n{60} {102}e{65}w{164} {79} {152}h{109}i{79}e{129}r{146} {143}e{176}e{104}n{98} {148}p{124}r{119}o{165}b{126}l{130}e{172}e{104}m{1032}{+KC_LSFT}{189};{140}{-KC_LSFT}";

static uint8_t     macro_buffer[TEST_MACRO_BUFFER_SIZE];
static uint16_t    fake_macro_buffer_size;
static uint8_t     fake_macro_count;
static test_call_t test_calls[TEST_MAX_CALLS];
static uint16_t    test_call_count;
static uint16_t    runtime_diag_heartbeat_count;

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
    fake_macro_buffer_size       = sizeof(macro_buffer);
    fake_macro_count             = DYNAMIC_KEYMAP_MACRO_COUNT;
    test_call_count              = 0;
    runtime_diag_heartbeat_count = 0;
    via_macro_provider_invalidate_all();
}

uint8_t dynamic_keymap_macro_get_count(void) {
    return fake_macro_count;
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

void send_char(char ascii_code) {
    (void)ascii_code;
    CHECK(false);
}

void wait_ms(uint16_t ms) {
    test_log_call(TEST_CALL_WAIT, ms, 0);
}

void noah_runtime_diag_heartbeat(void) {
    runtime_diag_heartbeat_count++;
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

bool pd_mode_toggle_lock_state_at(pd_mode_mask_t mode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_toggle_lock_state(mode);
}

bool pd_mode_handle_keycode_press(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool pd_mode_handle_keycode_press_at(uint16_t keycode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_handle_keycode_press(keycode);
}

bool pd_mode_handle_keycode_release(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool pd_mode_handle_keycode_release_at(uint16_t keycode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_handle_keycode_release(keycode);
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

void split_runtime_sync_request(void) {}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    (void)keycode;
    CHECK(false);
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    (void)keycode;
    CHECK(false);
}

bool noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    (void)keycode;
    (void)pressed;
    CHECK(false);
    return false;
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

    CHECK(test_call_count == 5);
    CHECK(test_calls[0].kind == TEST_CALL_WAIT);
    CHECK(test_calls[0].value == 100);
    CHECK(test_calls[1].kind == TEST_CALL_WAIT);
    CHECK(test_calls[1].value == 100);
    CHECK(test_calls[2].kind == TEST_CALL_WAIT);
    CHECK(test_calls[2].value == 50);
    CHECK(test_calls[3].kind == TEST_CALL_WAIT);
    CHECK(test_calls[3].value == TAP_CODE_DELAY);
    CHECK(test_calls[4].kind == TEST_CALL_SEND_CHAR);
    CHECK(test_calls[4].value == 'X');
    CHECK(test_calls[4].interval == TAP_CODE_DELAY);
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

static void test_slot_six_authored_alt_gui_8_chord_uses_owned_keycode_lifecycle(void) {
    test_reset_state();
    macro_buffer[0]  = 'A';
    macro_buffer[1]  = 0;
    macro_buffer[2]  = 'B';
    macro_buffer[3]  = 0;
    macro_buffer[4]  = 'C';
    macro_buffer[5]  = 0;
    macro_buffer[6]  = 'D';
    macro_buffer[7]  = 0;
    macro_buffer[8]  = 'E';
    macro_buffer[9]  = 0;
    macro_buffer[10] = 'F';
    macro_buffer[11] = 0;
    macro_buffer[12] = SS_QMK_PREFIX;
    macro_buffer[13] = SS_DOWN_CODE;
    macro_buffer[14] = KC_LEFT_ALT;
    macro_buffer[15] = SS_QMK_PREFIX;
    macro_buffer[16] = SS_DOWN_CODE;
    macro_buffer[17] = KC_LEFT_GUI;
    macro_buffer[18] = SS_QMK_PREFIX;
    macro_buffer[19] = SS_TAP_CODE;
    macro_buffer[20] = TEST_KC_8;
    macro_buffer[21] = SS_QMK_PREFIX;
    macro_buffer[22] = SS_UP_CODE;
    macro_buffer[23] = KC_LEFT_GUI;
    macro_buffer[24] = SS_QMK_PREFIX;
    macro_buffer[25] = SS_UP_CODE;
    macro_buffer[26] = KC_LEFT_ALT;

    noah_action_tap(QK_MACRO_0 + 6);

    CHECK(test_call_count == 6);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_REGISTER);
    CHECK(test_calls[0].value == KC_LEFT_ALT);
    CHECK(test_calls[1].kind == TEST_CALL_OWNED_REGISTER);
    CHECK(test_calls[1].value == KC_LEFT_GUI);
    CHECK(test_calls[2].kind == TEST_CALL_OWNED_TAP);
    CHECK(test_calls[2].value == TEST_KC_8);
    CHECK(test_calls[3].kind == TEST_CALL_OWNED_UNREGISTER);
    CHECK(test_calls[3].value == KC_LEFT_GUI);
    CHECK(test_calls[4].kind == TEST_CALL_OWNED_UNREGISTER);
    CHECK(test_calls[4].value == KC_LEFT_ALT);
    CHECK(test_calls[5].kind == TEST_CALL_WAIT);
    CHECK(test_calls[5].value == TAP_CODE_DELAY);
}

static void test_slot_six_long_delay_heavy_payload_replays_from_via_buffer(void) {
    uint16_t written       = 0;
    uint32_t total_wait_ms = 0;
    uint16_t delayed_chars = 0;
    uint16_t shift_downs   = 0;
    uint16_t shift_ups     = 0;
    uint16_t slot_offset   = 6;

    test_reset_state();
    CHECK(macro_payload_encode(test_long_delay_heavy_payload, &macro_buffer[slot_offset], (uint16_t)(sizeof(macro_buffer) - slot_offset), &written));
    CHECK(written > 0u);
    CHECK((uint16_t)(slot_offset + written) < sizeof(macro_buffer));
    macro_buffer[slot_offset + written] = 0;

    noah_action_tap(QK_MACRO_0 + 6);

    for (uint16_t index = 0; index < test_call_count; index++) {
        if (test_calls[index].kind == TEST_CALL_WAIT) {
            total_wait_ms += test_calls[index].value;
        } else if (test_calls[index].kind == TEST_CALL_SEND_CHAR) {
            delayed_chars++;
        } else if (test_calls[index].kind == TEST_CALL_OWNED_REGISTER && test_calls[index].value == KC_LEFT_SHIFT) {
            shift_downs++;
        } else if (test_calls[index].kind == TEST_CALL_OWNED_UNREGISTER && test_calls[index].value == KC_LEFT_SHIFT) {
            shift_ups++;
        }
    }

    CHECK(delayed_chars == 57u);
    CHECK(shift_downs == 1u);
    CHECK(shift_ups == 1u);
    CHECK(total_wait_ms == 13579u);
    CHECK(runtime_diag_heartbeat_count > 0u);
}

static void test_out_of_range_macro_slot_is_ignored(void) {
    test_reset_state();
    fake_macro_count = 2;
    macro_buffer[0]  = 'A';
    macro_buffer[1]  = 0;

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
    test_slot_six_authored_alt_gui_8_chord_uses_owned_keycode_lifecycle();
    test_slot_six_long_delay_heavy_payload_replays_from_via_buffer();
    test_out_of_range_macro_slot_is_ignored();
    test_zero_sized_macro_buffer_is_ignored();
    test_non_terminated_macro_buffer_is_ignored();
    test_unterminated_delay_command_matches_current_via_behavior();

    puts("via macro action_lifecycle host tests passed");
    return 0;
}
