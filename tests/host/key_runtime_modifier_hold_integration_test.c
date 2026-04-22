#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_runtime_integration_harness.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/key/interaction/key_behavior_lookup.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"
#include "users/noah/lib/state/runtime/runtime_reset.h"

enum {
    TEST_MULTI_TAP_KEY = SAFE_RANGE + 0x70,
};

static uint16_t fake_time;
static uint8_t  fake_mods;
static uint8_t  fake_weak_mods;
static uint8_t  fake_oneshot_mods;
static uint8_t  fake_oneshot_locked_mods;

static uint16_t integration_hold_modifier;
static uint8_t  integration_expected_mask;

static uint8_t               send_keyboard_report_count;
static uint8_t               emitted_action_count;
static uint16_t              last_emitted_action;
static uint8_t               last_emitted_mods;
static uint8_t               delayed_action_count;
static uint16_t              last_delayed_action;
static delayed_action_mods_t last_delayed_mods;

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

static keypos_t test_keypos(uint8_t row, uint8_t col) {
    return (keypos_t){
        .row = row,
        .col = col,
    };
}

static uint8_t test_snapshot_real_mods(void) {
    keyboard_mod_ownership_debug_snapshot_t snapshot = {0};

    keyboard_mod_ownership_debug_snapshot(&snapshot);
    return snapshot.live_state.real;
}

static void test_reset_state(void) {
    fake_time                  = 1000;
    fake_mods                  = 0;
    fake_weak_mods             = 0;
    fake_oneshot_mods          = 0;
    fake_oneshot_locked_mods   = 0;
    send_keyboard_report_count = 0;
    emitted_action_count       = 0;
    last_emitted_action        = KC_NO;
    last_emitted_mods          = 0;
    delayed_action_count       = 0;
    last_delayed_action        = KC_NO;
    last_delayed_mods          = (delayed_action_mods_t){0};
    noah_runtime_reset_for_test();
    send_keyboard_report_count = 0;
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

uint8_t get_mods(void) {
    return fake_mods;
}

uint8_t get_weak_mods(void) {
    return fake_weak_mods;
}

uint8_t get_oneshot_mods(void) {
    return fake_oneshot_mods;
}

uint8_t get_oneshot_locked_mods(void) {
    return fake_oneshot_locked_mods;
}

void set_mods(uint8_t mods) {
    fake_mods = mods;
}

void set_weak_mods(uint8_t mods) {
    fake_weak_mods = mods;
}

void set_oneshot_mods(uint8_t mods) {
    fake_oneshot_mods = mods;
}

void set_oneshot_locked_mods(uint8_t mods) {
    fake_oneshot_locked_mods = mods;
}

void clear_mods(void) {
    fake_mods = 0;
}

void clear_weak_mods(void) {
    fake_weak_mods = 0;
}

void clear_oneshot_mods(void) {
    fake_oneshot_mods = 0;
}

void clear_oneshot_locked_mods(void) {
    fake_oneshot_locked_mods = 0;
}

void add_mods(uint8_t mods) {
    fake_mods |= mods;
}

void del_mods(uint8_t mods) {
    fake_mods &= (uint8_t)~mods;
}

void send_keyboard_report(void) {
    send_keyboard_report_count++;
}

void register_code(uint8_t keycode) {
    (void)keycode;
}

void unregister_code(uint8_t keycode) {
    (void)keycode;
}

void register_code16(uint16_t keycode) {
    (void)keycode;
}

void unregister_code16(uint16_t keycode) {
    (void)keycode;
}

void tap_code16(uint16_t keycode) {
    (void)keycode;
}

void wait_ms(uint16_t ms) {
    (void)ms;
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    return (key_behavior_view_t){
        .keycode          = keycode,
        .handled          = true,
        .has_multi_tap    = keycode == TEST_MULTI_TAP_KEY,
        .tap_hold_term    = keycode == TEST_MULTI_TAP_KEY ? 120 : 0,
        .longer_hold_term = keycode == TEST_MULTI_TAP_KEY ? 240 : 0,
        .multi_tap_term   = keycode == TEST_MULTI_TAP_KEY ? 150 : 0,
    };
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    if (keycode == TEST_MULTI_TAP_KEY && tap_count == 3) {
        return (key_behavior_step_t){
            .hold = PRESS_AND_HOLD_UNTIL_RELEASE(integration_hold_modifier),
        };
    }

    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    return keycode == TEST_MULTI_TAP_KEY && count < 3;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    emitted_action_count++;
    last_emitted_action = action;
    last_emitted_mods   = fake_mods;
    (void)action;
    (void)policy;
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    delayed_action_count++;
    last_delayed_action = action;
    last_delayed_mods   = mods;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool pd_mode_toggle_lock_state_at(pd_mode_mask_t mode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_toggle_lock_state(mode);
}

void key_feedback_pulse_arm(bool long_hold_level) {
    (void)long_hold_level;
}

void split_runtime_sync(void) {}

void split_runtime_sync_request(void) {}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    (void)key_pos;
    (void)layer;
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

void noah_action_press(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

void noah_action_release(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

bool noah_synthetic_record_active(void) {
    return false;
}

bool macro_dispatch(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

static void test_assert_runtime_quiescent(keypos_t source_pos) {
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(source_pos) == KC_NO);
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));
}

static void test_begin_third_tap_hold_modifier_pending(keypos_t source_pos, uint16_t modifier, uint8_t expected_mask) {
    integration_hold_modifier = modifier;
    integration_expected_mask = expected_mask;
    test_reset_state();

    CHECK(!key_runtime_integration_process_record(TEST_MULTI_TAP_KEY, source_pos, true));
    CHECK(!key_runtime_integration_process_record(TEST_MULTI_TAP_KEY, source_pos, false));
    CHECK(noah_runtime_debug_slot_pending_multi_tap_count(source_pos) == 1);
    CHECK(get_mods() == 0);

    key_runtime_integration_advance(&fake_time, 40);
    CHECK(!key_runtime_integration_process_record(TEST_MULTI_TAP_KEY, source_pos, true));
    CHECK(!key_runtime_integration_process_record(TEST_MULTI_TAP_KEY, source_pos, false));
    CHECK(noah_runtime_debug_slot_pending_multi_tap_count(source_pos) == 2);
    CHECK(get_mods() == 0);

    key_runtime_integration_advance(&fake_time, 40);
    CHECK(!key_runtime_integration_process_record(TEST_MULTI_TAP_KEY, source_pos, true));
    CHECK(noah_runtime_debug_slot_pending_multi_tap_holding(source_pos));
    CHECK(noah_runtime_debug_slot_owner_keycode(source_pos) == TEST_MULTI_TAP_KEY);
    CHECK(get_mods() == 0);
}

static void test_activate_third_tap_hold_modifier(keypos_t source_pos, uint16_t modifier, uint8_t expected_mask) {
    test_begin_third_tap_hold_modifier_pending(source_pos, modifier, expected_mask);

    key_runtime_integration_advance(&fake_time, 130);
    key_runtime_integration_scan();

    CHECK(!noah_runtime_debug_slot_pending_multi_tap_holding(source_pos));
    CHECK(noah_runtime_debug_slot_held_action_keycode(source_pos) == modifier);
    CHECK((test_snapshot_real_mods() & integration_expected_mask) != 0);
    CHECK((get_mods() & integration_expected_mask) != 0);
    CHECK(send_keyboard_report_count == 1);
}

static void test_pending_third_tap_hold_modifier_keeps_processed_child_immediate(uint16_t modifier, uint8_t expected_mask) {
    keypos_t source_pos = test_keypos(1, 1);
    keypos_t child_pos  = test_keypos(1, 2);

    test_begin_third_tap_hold_modifier_pending(source_pos, modifier, expected_mask);

    CHECK(!key_runtime_integration_process_record(KC_C, child_pos, true));
    CHECK(!key_runtime_integration_process_record(KC_C, child_pos, false));
    CHECK(emitted_action_count == 1);
    CHECK(last_emitted_action == KC_C);
    CHECK(delayed_action_count == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);

    key_runtime_integration_advance(&fake_time, 130);
    key_runtime_integration_scan();
    CHECK(!noah_runtime_debug_slot_pending_multi_tap_holding(source_pos));
    CHECK(noah_runtime_debug_slot_held_action_keycode(source_pos) == modifier);
    CHECK((test_snapshot_real_mods() & expected_mask) != 0);
    CHECK((get_mods() & expected_mask) != 0);

    CHECK(!key_runtime_integration_process_record(TEST_MULTI_TAP_KEY, source_pos, false));
    CHECK((test_snapshot_real_mods() & expected_mask) == 0);
    CHECK((get_mods() & expected_mask) == 0);
    test_assert_runtime_quiescent(source_pos);
}

static void test_third_tap_hold_modifier_keeps_processed_child_immediate(uint16_t modifier, uint8_t expected_mask) {
    keypos_t source_pos = test_keypos(1, 1);
    keypos_t child_pos  = test_keypos(1, 2);

    test_activate_third_tap_hold_modifier(source_pos, modifier, expected_mask);

    CHECK(!key_runtime_integration_process_record(KC_C, child_pos, true));
    CHECK(!key_runtime_integration_process_record(KC_C, child_pos, false));
    CHECK(emitted_action_count == 1);
    CHECK(last_emitted_action == KC_C);
    CHECK((last_emitted_mods & expected_mask) != 0);
    CHECK(delayed_action_count == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);

    CHECK(!key_runtime_integration_process_record(TEST_MULTI_TAP_KEY, source_pos, false));
    CHECK((test_snapshot_real_mods() & expected_mask) == 0);
    CHECK((get_mods() & expected_mask) == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(source_pos) == KC_NO);
    CHECK(send_keyboard_report_count == 2);
    test_assert_runtime_quiescent(source_pos);

    CHECK(!key_runtime_integration_process_record(KC_V, child_pos, true));
    CHECK(!key_runtime_integration_process_record(KC_V, child_pos, false));
    CHECK(emitted_action_count == 2);
    CHECK(last_emitted_action == KC_V);
    CHECK((last_emitted_mods & expected_mask) == 0);
    CHECK(delayed_action_count == 0);
}

int main(void) {
    test_pending_third_tap_hold_modifier_keeps_processed_child_immediate(KC_LEFT_SHIFT, MOD_BIT(KC_LEFT_SHIFT));
    test_pending_third_tap_hold_modifier_keeps_processed_child_immediate(KC_LEFT_GUI, MOD_BIT(KC_LEFT_GUI));
    test_third_tap_hold_modifier_keeps_processed_child_immediate(KC_LEFT_SHIFT, MOD_BIT(KC_LEFT_SHIFT));
    test_third_tap_hold_modifier_keeps_processed_child_immediate(KC_LEFT_GUI, MOD_BIT(KC_LEFT_GUI));

    puts("key_runtime modifier-hold integration tests passed");
    return 0;
}
