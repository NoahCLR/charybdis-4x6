#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/owned_keycode.h"
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
static uint16_t    macro_buffer_read_count;
static uint32_t    fake_time;

const uint8_t ascii_to_shift_lut[16];
const uint8_t ascii_to_altgr_lut[16];
const uint8_t ascii_to_dead_lut[16];
const uint8_t ascii_to_keycode_lut[128] = {
    ['A'] = KC_A, ['B'] = KC_B, ['C'] = KC_C, ['D'] = KC_D, ['E'] = KC_E, ['F'] = KC_F, ['H'] = KC_H, ['X'] = KC_X, ['i'] = KC_I,
};

uint32_t timer_read32(void) {
    return fake_time;
}

uint32_t timer_elapsed32(uint32_t last) {
    return fake_time - last;
}

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
    macro_payload_debug_snapshot_t snapshot;

    macro_payload_debug_snapshot(&snapshot);
    if (snapshot.state != MACRO_PAYLOAD_ENGINE_IDLE) {
        (void)macro_payload_engine_cancel();
        for (uint8_t scan = 0u; scan < 64u; scan++) {
            macro_payload_engine_scan();
            fake_time += 100000u;
            macro_payload_debug_snapshot(&snapshot);
            if (snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE) {
                break;
            }
        }
        CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE);
    }
    macro_payload_engine_init();
    memset(macro_buffer, 0, sizeof(macro_buffer));
    memset(test_calls, 0, sizeof(test_calls));
    fake_macro_buffer_size       = sizeof(macro_buffer);
    fake_macro_count             = DYNAMIC_KEYMAP_MACRO_COUNT;
    test_call_count              = 0;
    runtime_diag_heartbeat_count = 0;
    macro_buffer_read_count      = 0;
    fake_time                    = 1000u;
    via_macro_provider_invalidate_all();
}

static void test_run_macro_to_idle(void) {
    macro_payload_debug_snapshot_t snapshot;

    for (uint16_t scan = 0u; scan < 4096u; scan++) {
        macro_payload_engine_scan();
        fake_time += 100000u;
        macro_payload_debug_snapshot(&snapshot);
        if (snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE) {
            return;
        }
    }
    CHECK(false);
}

uint8_t dynamic_keymap_macro_get_count(void) {
    return fake_macro_count;
}

uint16_t dynamic_keymap_macro_get_buffer_size(void) {
    return fake_macro_buffer_size;
}

void dynamic_keymap_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    macro_buffer_read_count++;
    memcpy(data, &macro_buffer[offset], size);
}

bool owned_keycode_tap(uint16_t keycode) {
    test_log_call(TEST_CALL_OWNED_TAP, keycode, 0);
    return true;
}

bool owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease) {
    CHECK(lease != NULL);
    test_log_call(TEST_CALL_OWNED_REGISTER, keycode, 0);
    *lease = (owned_keycode_lease_t){.active = true, .has_basic = true, .basic = (uint8_t)keycode};
    return true;
}

bool owned_keycode_release(owned_keycode_lease_t *lease) {
    CHECK(lease != NULL);
    CHECK(lease->active);
    test_log_call(TEST_CALL_OWNED_UNREGISTER, lease->basic, 0);
    *lease = (owned_keycode_lease_t){0};
    return true;
}

bool owned_keycode_register(uint16_t keycode) {
    owned_keycode_lease_t lease = {0};

    return owned_keycode_acquire(keycode, &lease);
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

static void test_qmk_tap_command_uses_scan_driven_owned_lease(void) {
    test_reset_state();
    macro_buffer[0] = SS_QMK_PREFIX;
    macro_buffer[1] = SS_TAP_CODE;
    macro_buffer[2] = KC_C;

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 0u);
    test_run_macro_to_idle();
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_REGISTER);
    CHECK(test_calls[0].value == KC_C);
    CHECK(test_calls[1].kind == TEST_CALL_OWNED_UNREGISTER);
    CHECK(test_calls[1].value == KC_C);
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

    CHECK(test_call_count == 0u);
    test_run_macro_to_idle();
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_REGISTER);
    CHECK(test_calls[0].value == KC_LEFT_SHIFT);
    CHECK(test_calls[1].kind == TEST_CALL_OWNED_UNREGISTER);
    CHECK(test_calls[1].value == KC_LEFT_SHIFT);
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

    CHECK(test_call_count == 0u);
    test_run_macro_to_idle();
    CHECK(test_call_count == 2u);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_REGISTER);
    CHECK(test_calls[0].value == KC_X);
    CHECK(test_calls[1].kind == TEST_CALL_OWNED_UNREGISTER);
    CHECK(test_calls[1].value == KC_X);
}

static void test_plain_text_uses_scan_driven_ascii_leases(void) {
    test_reset_state();
    macro_buffer[0] = 'H';
    macro_buffer[1] = 'i';

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 0u);
    test_run_macro_to_idle();
    CHECK(test_call_count == 4u);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_REGISTER && test_calls[0].value == KC_H);
    CHECK(test_calls[1].kind == TEST_CALL_OWNED_UNREGISTER && test_calls[1].value == KC_H);
    CHECK(test_calls[2].kind == TEST_CALL_OWNED_REGISTER && test_calls[2].value == KC_I);
    CHECK(test_calls[3].kind == TEST_CALL_OWNED_UNREGISTER && test_calls[3].value == KC_I);
}

static void test_macro_slot_lookup_skips_null_terminated_entries(void) {
    test_reset_state();
    macro_buffer[0] = 'A';
    macro_buffer[1] = 0;
    macro_buffer[2] = SS_QMK_PREFIX;
    macro_buffer[3] = SS_TAP_CODE;
    macro_buffer[4] = KC_V;

    noah_action_tap(QK_MACRO_0 + 1);

    CHECK(test_call_count == 0u);
    test_run_macro_to_idle();
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_REGISTER);
    CHECK(test_calls[0].value == KC_V);
    CHECK(test_calls[1].kind == TEST_CALL_OWNED_UNREGISTER);
    CHECK(test_calls[1].value == KC_V);
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

    CHECK(test_call_count == 0u);
    test_run_macro_to_idle();
    CHECK(test_call_count == 6);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_REGISTER);
    CHECK(test_calls[0].value == KC_LEFT_ALT);
    CHECK(test_calls[1].kind == TEST_CALL_OWNED_REGISTER);
    CHECK(test_calls[1].value == KC_LEFT_GUI);
    CHECK(test_calls[2].kind == TEST_CALL_OWNED_REGISTER);
    CHECK(test_calls[2].value == TEST_KC_8);
    CHECK(test_calls[3].kind == TEST_CALL_OWNED_UNREGISTER && test_calls[3].value == TEST_KC_8);
    CHECK(test_calls[4].kind == TEST_CALL_OWNED_UNREGISTER && test_calls[4].value == KC_LEFT_GUI);
    CHECK(test_calls[5].kind == TEST_CALL_OWNED_UNREGISTER && test_calls[5].value == KC_LEFT_ALT);
}

static void test_slot_six_long_delay_heavy_payload_replays_from_via_buffer(void) {
    uint16_t written       = 0;
    uint16_t registrations = 0;
    uint16_t releases      = 0;
    uint16_t shift_downs   = 0;
    uint16_t shift_ups     = 0;
    uint16_t slot_offset   = 6;

    test_reset_state();
    CHECK(macro_payload_encode(test_long_delay_heavy_payload, &macro_buffer[slot_offset], (uint16_t)(sizeof(macro_buffer) - slot_offset), &written));
    CHECK(written > 0u);
    CHECK((uint16_t)(slot_offset + written) < sizeof(macro_buffer));
    macro_buffer[slot_offset + written] = 0;

    noah_action_tap(QK_MACRO_0 + 6);
    CHECK(test_call_count == 0u);
    test_run_macro_to_idle();

    for (uint16_t index = 0; index < test_call_count; index++) {
        CHECK(test_calls[index].kind != TEST_CALL_WAIT);
        if (test_calls[index].kind == TEST_CALL_OWNED_REGISTER) {
            registrations++;
            if (test_calls[index].value == KC_LEFT_SHIFT) {
                shift_downs++;
            }
        } else if (test_calls[index].kind == TEST_CALL_OWNED_UNREGISTER) {
            releases++;
            if (test_calls[index].value == KC_LEFT_SHIFT) {
                shift_ups++;
            }
        }
    }

    CHECK(registrations == 58u);
    CHECK(releases == 58u);
    CHECK(shift_downs == 1u);
    CHECK(shift_ups == 1u);
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

    CHECK(test_call_count == 0u);
    test_run_macro_to_idle();
    CHECK(test_call_count == 0u);
}

static void test_invalid_high_text_is_cached_until_explicit_invalidation(void) {
    uint16_t reads_after_rejection;

    test_reset_state();
    macro_buffer[0] = 0x80u;

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 0u);
    CHECK(macro_buffer_read_count > 0u);
    reads_after_rejection = macro_buffer_read_count;

    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 0u);
    CHECK(macro_buffer_read_count == reads_after_rejection);

    macro_buffer[0] = 'A';
    noah_action_tap(QK_MACRO_0);

    CHECK(test_call_count == 0u);
    CHECK(macro_buffer_read_count == reads_after_rejection);

    via_macro_provider_invalidate_all();
    noah_action_tap(QK_MACRO_0);

    CHECK(macro_buffer_read_count > reads_after_rejection);
    CHECK(test_call_count == 0u);
    test_run_macro_to_idle();
    CHECK(test_call_count == 2u);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_REGISTER && test_calls[0].value == KC_A);
    CHECK(test_calls[1].kind == TEST_CALL_OWNED_UNREGISTER && test_calls[1].value == KC_A);
}

static void test_invalidation_keeps_active_ir_immutable_then_reloads(void) {
    macro_payload_debug_snapshot_t snapshot;

    test_reset_state();
    macro_buffer[0] = SS_QMK_PREFIX;
    macro_buffer[1] = SS_DELAY_CODE;
    macro_buffer[2] = '1';
    macro_buffer[3] = '0';
    macro_buffer[4] = '0';
    macro_buffer[5] = '0';
    macro_buffer[6] = '|';
    macro_buffer[7] = 'A';

    noah_action_tap(QK_MACRO_0);
    macro_payload_engine_scan();
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_WAITING);

    memset(macro_buffer, 0, sizeof(macro_buffer));
    macro_buffer[0] = 'B';
    via_macro_provider_invalidate_all();
    test_run_macro_to_idle();
    CHECK(test_call_count == 2u);
    CHECK(test_calls[0].value == KC_A);
    CHECK(test_calls[1].value == KC_A);

    test_call_count = 0u;
    noah_action_tap(QK_MACRO_0);
    test_run_macro_to_idle();
    CHECK(test_call_count == 2u);
    CHECK(test_calls[0].value == KC_B);
    CHECK(test_calls[1].value == KC_B);
}

static void test_busy_via_trigger_is_consumed_without_restarting(void) {
    macro_payload_debug_snapshot_t snapshot;

    test_reset_state();
    macro_buffer[0] = SS_QMK_PREFIX;
    macro_buffer[1] = SS_DELAY_CODE;
    macro_buffer[2] = '1';
    macro_buffer[3] = '0';
    macro_buffer[4] = '0';
    macro_buffer[5] = '0';
    macro_buffer[6] = '|';
    macro_buffer[7] = 'A';
    macro_buffer[8] = 0u;
    macro_buffer[9] = 'B';

    noah_action_tap(QK_MACRO_0);
    noah_action_tap(QK_MACRO_0 + 1u);
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.busy_rejection_count == 1u);
    CHECK(snapshot.active_slot == 0u);

    test_run_macro_to_idle();
    CHECK(test_call_count == 2u);
    CHECK(test_calls[0].value == KC_A);
    CHECK(test_calls[1].value == KC_A);
}

int main(void) {
    test_qmk_tap_command_uses_scan_driven_owned_lease();
    test_qmk_down_and_up_commands_use_owned_register_and_unregister();
    test_delay_command_matches_upstream_parsing();
    test_plain_text_uses_scan_driven_ascii_leases();
    test_macro_slot_lookup_skips_null_terminated_entries();
    test_slot_six_authored_alt_gui_8_chord_uses_owned_keycode_lifecycle();
    test_slot_six_long_delay_heavy_payload_replays_from_via_buffer();
    test_out_of_range_macro_slot_is_ignored();
    test_zero_sized_macro_buffer_is_ignored();
    test_non_terminated_macro_buffer_is_ignored();
    test_unterminated_delay_command_matches_current_via_behavior();
    test_invalid_high_text_is_cached_until_explicit_invalidation();
    test_invalidation_keeps_active_ir_immutable_then_reloads();
    test_busy_via_trigger_is_consumed_without_restarting();

    puts("via macro action_lifecycle host tests passed");
    return 0;
}
