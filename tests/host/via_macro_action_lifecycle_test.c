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
// The keyboard's whole shared pool (D-F14: a 36 KiB VIA region).
#define TEST_MACRO_BUFFER_SIZE 34903

static const char *const test_long_delay_heavy_payload = "h{829}e{627}y{665} {249}h{158}a{167}l{424}o{386} {448}h{144}o{111}e{103} {118}i{123}s{118} {125}h{132}e{134}t{158} {133}m{118}e{493}t{156} {503}y{503}u{10}o{695} {382}h{113}e{212}b{149}b{83}e{155}n{60} {102}e{65}w{164} {79} {152}h{109}i{79}e{129}r{146} {143}e{176}e{104}n{98} {148}p{124}r{119}o{165}b{126}l{130}e{172}e{104}m{1032}{+KC_LSFT}{189};{140}{-KC_LSFT}";

static uint8_t     macro_buffer[TEST_MACRO_BUFFER_SIZE];
static uint16_t    fake_macro_buffer_size;
static uint8_t     fake_macro_count;
static test_call_t test_calls[TEST_MAX_CALLS];
static uint16_t    test_call_count;
static uint16_t    runtime_diag_heartbeat_count;
static uint32_t    macro_buffer_read_count;
// Long playbacks only count presses and fold their keycodes into a digest.
static bool        test_count_only;
static uint32_t    test_press_count;
static uint32_t    test_release_count;
static uint32_t    test_press_digest;
static uint32_t    fake_time;

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
    if (test_count_only) {
        if (kind == TEST_CALL_OWNED_REGISTER) {
            test_press_count++;
            test_press_digest = test_press_digest * 31u + value;
        } else if (kind == TEST_CALL_OWNED_UNREGISTER) {
            test_release_count++;
        }
        return;
    }
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
    test_count_only              = false;
    test_press_count             = 0u;
    test_release_count           = 0u;
    test_press_digest            = 0u;
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

// The ledger is faked here, so a literal tap is recorded as QMK's tap.
void owned_keycode_tap_literal(uint16_t keycode) {
    tap_code16(keycode);
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

bool layer_ownership_goto(uint8_t layer) {
    (void)layer;
    return false;
}

bool is_oneshot_enabled(void) {
    return true;
}

uint16_t timer_read(void) {
    return 0u;
}

bool layer_ownership_oneshot_tap(uint8_t layer, uint16_t now, uint16_t double_tap_ms) {
    (void)now;
    (void)double_tap_ms;
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

void pointer_layer_policy_sync_layer_ownership_anchor(void) {}

void pointer_layer_policy_settle_record(void) {}

void pointer_layer_policy_take_back_qmk_toggle(uint16_t keycode, const keyrecord_t *record) {
    (void)keycode;
    (void)record;
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
    macro_buffer[6] = 'x';

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
    macro_buffer[0] = 'h';
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
    macro_buffer[0] = 'a';
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
    macro_buffer[10] = 'f';
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

    macro_buffer[0] = 'a';
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

static void test_valid_slot_redecodes_once_per_playback(void) {
    uint16_t first_play_reads;

    test_reset_state();
    macro_buffer[0] = 'a';

    noah_action_tap(QK_MACRO_0);
    first_play_reads = macro_buffer_read_count;
    CHECK(first_play_reads > 0u);
    CHECK(first_play_reads <= fake_macro_buffer_size);
    test_run_macro_to_idle();

    noah_action_tap(QK_MACRO_0);
    CHECK(macro_buffer_read_count - first_play_reads == first_play_reads);
    test_run_macro_to_idle();
    CHECK(test_call_count == 4u);
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
    macro_buffer[7] = 'a';

    noah_action_tap(QK_MACRO_0);
    macro_payload_engine_scan();
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_WAITING);

    memset(macro_buffer, 0, sizeof(macro_buffer));
    macro_buffer[0] = 'b';
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
    macro_buffer[7] = 'a';
    macro_buffer[8] = 0u;
    macro_buffer[9] = 'b';

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

// A full pool: macros 0..126 fill it and macro 127, the last slot, ends at the
// pool's last bytes. Starting it walks the pool once to find it; the read
// count is that cost, which the hardware acceptance times.
static void test_last_slot_starts_on_a_full_pool(void) {
    const uint16_t filler = (uint16_t)((TEST_MACRO_BUFFER_SIZE - 3u - 127u) / 127u);
    uint16_t       offset = 0;

    test_reset_state();
    for (uint8_t slot = 0; slot < 127; slot++) {
        for (uint16_t i = 0; i < filler; i++) macro_buffer[offset + i] = 'a';
        offset = (uint16_t)(offset + filler);
        macro_buffer[offset++] = 0;
    }
    macro_buffer[offset++] = SS_QMK_PREFIX;
    macro_buffer[offset++] = SS_TAP_CODE;
    macro_buffer[offset++] = KC_Z;
    CHECK(offset <= TEST_MACRO_BUFFER_SIZE - 1u && macro_buffer[TEST_MACRO_BUFFER_SIZE - 1u] == 0);

    macro_buffer_read_count = 0;
    noah_action_tap(QK_MACRO_0 + 127);
    test_run_macro_to_idle();
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_OWNED_REGISTER && test_calls[0].value == KC_Z);
    CHECK(test_calls[1].kind == TEST_CALL_OWNED_UNREGISTER && test_calls[1].value == KC_Z);
    printf("macro 127 on a full %u-byte pool: %u pool reads to find and play it\n", (unsigned)TEST_MACRO_BUFFER_SIZE, (unsigned)macro_buffer_read_count);
    CHECK(macro_buffer_read_count <= TEST_MACRO_BUFFER_SIZE + 16u);
}

// Fills slot `slot` from `offset` with `count` letters a..z, then terminates
// it. Returns the offset after the terminator and folds the expected presses.
static uint16_t test_write_letters(uint16_t offset, uint16_t count, uint32_t *digest) {
    for (uint16_t i = 0u; i < count; i++) {
        uint8_t letter         = (uint8_t)('a' + i % 26u);
        macro_buffer[offset++] = letter;
        if (digest) {
            *digest = *digest * 31u + (uint16_t)(KC_A + (letter - 'a'));
        }
    }
    macro_buffer[offset++] = 0u;
    return offset;
}

static void test_run_long_macro_to_idle(uint32_t max_scans) {
    macro_payload_debug_snapshot_t snapshot;

    for (uint32_t scan = 0u; scan < max_scans; scan++) {
        macro_payload_engine_scan();
        fake_time += 1000u;
        macro_payload_debug_snapshot(&snapshot);
        if (snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE) {
            return;
        }
    }
    CHECK(false);
}

// One macro after a long first one fills the rest of the bank and plays to its
// end; finding it again for each window would read the bank once per window.
static void test_macro_filling_the_bank_plays_to_its_end(void) {
    macro_payload_debug_snapshot_t snapshot;
    uint32_t                       expected = 0u;
    uint16_t                       offset;
    uint16_t                       letters;

    test_reset_state();
    offset  = test_write_letters(0u, 17000u, NULL);
    letters = (uint16_t)(TEST_MACRO_BUFFER_SIZE - offset - 1u - 126u);
    offset  = test_write_letters(offset, letters, &expected);
    CHECK(offset == TEST_MACRO_BUFFER_SIZE - 126u);

    test_count_only = true;
    noah_action_tap(QK_MACRO_0 + 1u);
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state != MACRO_PAYLOAD_ENGINE_IDLE);
    test_run_long_macro_to_idle(400000u);

    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.last_finish == MACRO_PAYLOAD_FINISH_SUCCESS);
    CHECK(test_press_count == letters && test_release_count == letters);
    CHECK(test_press_digest == expected);
    printf("a %u-letter macro after a 17000-byte one: %u bank reads to check and play it\n", (unsigned)letters, (unsigned)macro_buffer_read_count);
    CHECK(macro_buffer_read_count <= 4u * TEST_MACRO_BUFFER_SIZE);
}

static void test_single_macro_uses_all_free_bank_space(void) {
    test_reset_state();
    uint32_t expected = 0u;
    const uint16_t letters = TEST_MACRO_BUFFER_SIZE - DYNAMIC_KEYMAP_MACRO_COUNT;
    CHECK(test_write_letters(0u, letters, &expected) == letters + 1u);
    test_count_only = true;
    noah_action_tap(QK_MACRO_0);
    test_run_long_macro_to_idle(400000u);
    macro_payload_debug_snapshot_t snapshot;
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.last_finish == MACRO_PAYLOAD_FINISH_SUCCESS);
    CHECK(test_press_count == letters && test_release_count == letters);
    CHECK(test_press_digest == expected);
}

static void test_invalid_byte_late_in_a_long_macro_types_nothing(void) {
    macro_payload_debug_snapshot_t snapshot;
    uint16_t                       offset;

    test_reset_state();
    offset                    = test_write_letters(0u, 20000u, NULL);
    macro_buffer[offset - 1u] = 0x80u;
    macro_buffer[offset]      = 0u;
    test_count_only            = true;
    noah_action_tap(QK_MACRO_0);
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE);
    CHECK(test_press_count == 0u);

    // A key still held when a long macro ends is refused before typing too.
    test_reset_state();
    macro_buffer[0] = SS_QMK_PREFIX;
    macro_buffer[1] = SS_DOWN_CODE;
    macro_buffer[2] = KC_LEFT_SHIFT;
    (void)test_write_letters(3u, 20000u, NULL);
    test_count_only = true;
    noah_action_tap(QK_MACRO_0);
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.state == MACRO_PAYLOAD_ENGINE_IDLE);
    CHECK(test_press_count == 0u);
}

// A write to the bank stops a macro still reading it: what it holds is
// released and nothing from the new bytes is typed.
static void test_bank_write_cancels_a_macro_still_reading_it(void) {
    macro_payload_debug_snapshot_t snapshot;
    uint16_t                       offset;
    uint32_t                       presses_at_cancel;

    test_reset_state();
    macro_buffer[0]          = SS_QMK_PREFIX;
    macro_buffer[1]          = SS_DOWN_CODE;
    macro_buffer[2]          = KC_LEFT_SHIFT;
    offset                   = (uint16_t)(test_write_letters(3u, 20000u, NULL) - 1u);
    macro_buffer[offset++]   = SS_QMK_PREFIX;
    macro_buffer[offset++]   = SS_UP_CODE;
    macro_buffer[offset++]   = KC_LEFT_SHIFT;
    macro_buffer[offset]     = 0u;

    test_count_only = true;
    noah_action_tap(QK_MACRO_0);
    for (uint16_t scan = 0u; scan < 2000u; scan++) {
        macro_payload_engine_scan();
        fake_time += 1000u;
    }
    CHECK(test_press_count > 100u);

    via_macro_provider_storage_changing();
    presses_at_cancel = test_press_count;
    memset(macro_buffer, 'z', 30000u);
    test_run_long_macro_to_idle(1000u);

    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.last_finish == MACRO_PAYLOAD_FINISH_CANCELLED);
    CHECK(test_press_count == presses_at_cancel);
    CHECK(test_press_count == test_release_count);
    CHECK(snapshot.cancellation_count == 1u);
    memset(macro_buffer, 0, sizeof(macro_buffer));
    macro_buffer[0] = 'a';
    via_macro_provider_invalidate_all();
    noah_action_tap(QK_MACRO_0);
    test_run_long_macro_to_idle(1000u);
    macro_payload_debug_snapshot(&snapshot);
    CHECK(snapshot.last_finish == MACRO_PAYLOAD_FINISH_SUCCESS);
    CHECK(test_press_count == presses_at_cancel + 1u);
}

int main(void) {
    test_qmk_tap_command_uses_scan_driven_owned_lease();
    test_qmk_down_and_up_commands_use_owned_register_and_unregister();
    test_delay_command_matches_upstream_parsing();
    test_plain_text_uses_scan_driven_ascii_leases();
    test_macro_slot_lookup_skips_null_terminated_entries();
    test_last_slot_starts_on_a_full_pool();
    test_slot_six_authored_alt_gui_8_chord_uses_owned_keycode_lifecycle();
    test_slot_six_long_delay_heavy_payload_replays_from_via_buffer();
    test_out_of_range_macro_slot_is_ignored();
    test_zero_sized_macro_buffer_is_ignored();
    test_non_terminated_macro_buffer_is_ignored();
    test_unterminated_delay_command_matches_current_via_behavior();
    test_invalid_high_text_is_cached_until_explicit_invalidation();
    test_valid_slot_redecodes_once_per_playback();
    test_invalidation_keeps_active_ir_immutable_then_reloads();
    test_busy_via_trigger_is_consumed_without_restarting();
    test_macro_filling_the_bank_plays_to_its_end();
    test_single_macro_uses_all_free_bank_space();
    test_invalid_byte_late_in_a_long_macro_types_nothing();
    test_bank_write_cancels_a_macro_still_reading_it();

    puts("via macro action_lifecycle host tests passed");
    return 0;
}

void send_keyboard_report(void) {}
