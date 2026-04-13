#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/key/runtime/key_runtime_process.h"
#include "users/noah/lib/key/runtime/key_runtime_state.h"

enum {
    TEST_MULTI_TAP_KEY = SAFE_RANGE + 0x70,
    TEST_CHORD_KEY     = KC_C,
};

static uint16_t fake_time;
static uint8_t  fake_mods;
static uint8_t  fake_weak_mods;
static uint8_t  fake_oneshot_mods;
static uint8_t  fake_oneshot_locked_mods;

static uint16_t integration_hold_modifier;
static uint8_t  integration_expected_mask;

static uint8_t send_keyboard_report_count;
static uint8_t register_code_count;
static uint8_t unregister_code_count;
static uint8_t last_registered_keycode;
static uint8_t last_registered_mods;
static uint8_t last_unregistered_keycode;
static uint8_t last_unregistered_mods;

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

static keyrecord_t test_record(keypos_t key_pos, bool pressed) {
    keyrecord_t record = {
        .event =
            {
                .key     = key_pos,
                .pressed = pressed,
            },
    };

    return record;
}

static handled_key_view_t test_handled_key(void) {
    handled_key_view_t key = handled_key_lookup(TEST_MULTI_TAP_KEY);
    key.flags |= HANDLED_KEY_FLAG_MULTI_TAP;
    key.tap_hold_term    = 120;
    key.longer_hold_term = 240;
    key.multi_tap_term   = 150;
    return key;
}

static void test_reset_state(void) {
    fake_time                  = 1000;
    fake_mods                  = 0;
    fake_weak_mods             = 0;
    fake_oneshot_mods          = 0;
    fake_oneshot_locked_mods   = 0;
    send_keyboard_report_count = 0;
    register_code_count        = 0;
    unregister_code_count      = 0;
    last_registered_keycode    = KC_NO;
    last_registered_mods       = 0;
    last_unregistered_keycode  = KC_NO;
    last_unregistered_mods     = 0;
    noah_runtime_shared_state  = (runtime_shared_state_t){0};
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
    register_code_count++;
    last_registered_keycode = keycode;
    last_registered_mods    = fake_mods;
}

void unregister_code(uint8_t keycode) {
    unregister_code_count++;
    last_unregistered_keycode = keycode;
    last_unregistered_mods    = fake_mods;
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
        .keycode = keycode,
        .handled = true,
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
    (void)action;
    (void)mods;
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

void key_feedback_pulse_arm(bool long_hold_level) {
    (void)long_hold_level;
}

void split_runtime_sync(void) {}

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

static void test_third_tap_hold_modifier_applies_to_chorded_key(uint16_t modifier, uint8_t expected_mask) {
    handled_key_view_t  key            = test_handled_key();
    keypos_t            source_pos     = test_keypos(1, 1);
    keyrecord_t         press_record   = test_record(source_pos, true);
    keyrecord_t         release_record = test_record(source_pos, false);
    active_key_state_t *slot           = key_runtime_slot_for_position(source_pos);
    multi_tap_t        *slot_multi_tap = key_runtime_multi_tap_for_slot(slot);

    integration_hold_modifier = modifier;
    integration_expected_mask = expected_mask;
    test_reset_state();

    CHECK(key_runtime_process_handled_key_press(TEST_MULTI_TAP_KEY, &press_record, key));
    CHECK(key_runtime_process_handled_key_release(TEST_MULTI_TAP_KEY, &release_record, key));
    CHECK(slot_multi_tap->count == 1);
    CHECK(get_mods() == 0);

    fake_time = (uint16_t)(fake_time + 40);
    CHECK(key_runtime_process_handled_key_press(TEST_MULTI_TAP_KEY, &press_record, key));
    CHECK(key_runtime_process_handled_key_release(TEST_MULTI_TAP_KEY, &release_record, key));
    CHECK(slot_multi_tap->count == 2);
    CHECK(get_mods() == 0);

    fake_time = (uint16_t)(fake_time + 40);
    CHECK(key_runtime_process_handled_key_press(TEST_MULTI_TAP_KEY, &press_record, key));
    CHECK(slot_multi_tap->pending_hold);
    CHECK(slot->owner.keycode == TEST_MULTI_TAP_KEY);
    CHECK(get_mods() == 0);

    fake_time = (uint16_t)(fake_time + 130);
    noah_key_runtime_scan();

    CHECK(!slot_multi_tap->pending_hold);
    CHECK(slot->lifecycle.held_action_keycode == modifier);
    CHECK((get_mods() & integration_expected_mask) != 0);
    CHECK(send_keyboard_report_count == 1);

    CHECK(owned_keycode_register(TEST_CHORD_KEY));
    CHECK(last_registered_keycode == TEST_CHORD_KEY);
    CHECK((last_registered_mods & integration_expected_mask) != 0);

    CHECK(owned_keycode_unregister(TEST_CHORD_KEY));
    CHECK(last_unregistered_keycode == TEST_CHORD_KEY);
    CHECK((last_unregistered_mods & integration_expected_mask) != 0);

    CHECK(key_runtime_process_handled_key_release(TEST_MULTI_TAP_KEY, &release_record, key));
    CHECK((get_mods() & integration_expected_mask) == 0);
    CHECK(slot->owner.keycode == KC_NO);
    CHECK(send_keyboard_report_count == 2);

    CHECK(owned_keycode_register(TEST_CHORD_KEY));
    CHECK(last_registered_keycode == TEST_CHORD_KEY);
    CHECK((last_registered_mods & integration_expected_mask) == 0);
    CHECK(owned_keycode_unregister(TEST_CHORD_KEY));
}

int main(void) {
    test_third_tap_hold_modifier_applies_to_chorded_key(KC_LEFT_SHIFT, MOD_BIT(KC_LEFT_SHIFT));
    test_third_tap_hold_modifier_applies_to_chorded_key(KC_LEFT_GUI, MOD_BIT(KC_LEFT_GUI));

    puts("key_runtime modifier-hold integration tests passed");
    return 0;
}
