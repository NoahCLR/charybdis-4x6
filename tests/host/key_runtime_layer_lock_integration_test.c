#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/key/delayed_action.h"
#include "users/noah/lib/key/key_runtime_process.h"
#include "users/noah/lib/key/key_runtime_state.h"
#include "users/noah/lib/pointing/pd_modes.h"
#include "users/noah/lib/state/layer_ownership.h"
#include "users/noah/noah_keymap_ids.h"
#include "users/noah/noah_runtime.h"

enum {
    TEST_MULTI_TAP_KEY = SAFE_RANGE + 0x70,
    TEST_ALT_ACTION    = SAFE_RANGE + 0x71,
    TEST_FOREIGN_RELEASE_KEY = 0x0004u,
    TEST_NUM_LAYER     = 1,
    TEST_OTHER_LAYER   = 2,
};

layer_state_t layer_state;

static uint16_t fake_time;

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
    return (keyrecord_t){
        .event =
            {
                .key     = key_pos,
                .pressed = pressed,
            },
    };
}

static layer_state_t test_layer_mask(uint8_t layer) {
    return (layer_state_t)1u << layer;
}

static void test_reset_state(void) {
    fake_time                 = 1000;
    layer_state               = 0;
    noah_runtime_shared_state = (runtime_shared_state_t){0};
    layer_ownership_reset_for_test();
}

static void test_run_thumb_like_double_tap_hold_cycle_with_release_keycode(keypos_t key_pos, uint16_t release_keycode) {
    keyrecord_t press_record   = test_record(key_pos, true);
    keyrecord_t release_record = test_record(key_pos, false);

    CHECK(!noah_process_record_user(TEST_MULTI_TAP_KEY, &press_record));
    CHECK(!noah_process_record_user(TEST_MULTI_TAP_KEY, &release_record));

    fake_time = (uint16_t)(fake_time + 40);
    CHECK(!noah_process_record_user(TEST_MULTI_TAP_KEY, &press_record));

    fake_time = (uint16_t)(fake_time + 360);
    noah_key_runtime_scan();

    CHECK(!noah_process_record_user(release_keycode, &release_record));
}

static void test_run_thumb_like_double_tap_hold_cycle_with_intermediate_scan(keypos_t key_pos, uint16_t release_keycode, uint16_t pre_threshold_scan_ms, uint16_t hold_ms) {
    keyrecord_t press_record   = test_record(key_pos, true);
    keyrecord_t release_record = test_record(key_pos, false);

    CHECK(!noah_process_record_user(TEST_MULTI_TAP_KEY, &press_record));
    CHECK(!noah_process_record_user(TEST_MULTI_TAP_KEY, &release_record));

    fake_time = (uint16_t)(fake_time + 40);
    CHECK(!noah_process_record_user(TEST_MULTI_TAP_KEY, &press_record));

    fake_time = (uint16_t)(fake_time + pre_threshold_scan_ms);
    noah_key_runtime_scan();

    fake_time = (uint16_t)(fake_time + hold_ms);
    noah_key_runtime_scan();

    CHECK(!noah_process_record_user(release_keycode, &release_record));
}

static void test_run_thumb_like_double_tap_hold_cycle(keypos_t key_pos) {
    test_run_thumb_like_double_tap_hold_cycle_with_release_keycode(key_pos, TEST_MULTI_TAP_KEY);
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

uint32_t timer_read32(void) {
    return fake_time;
}

uint32_t timer_elapsed32(uint32_t last) {
    return (uint32_t)(fake_time - last);
}

uint8_t get_mods(void) {
    return 0;
}

uint8_t get_weak_mods(void) {
    return 0;
}

uint8_t get_oneshot_mods(void) {
    return 0;
}

uint8_t get_oneshot_locked_mods(void) {
    return 0;
}

void set_mods(uint8_t mods) {
    (void)mods;
}

void set_weak_mods(uint8_t mods) {
    (void)mods;
}

void set_oneshot_mods(uint8_t mods) {
    (void)mods;
}

void set_oneshot_locked_mods(uint8_t mods) {
    (void)mods;
}

void clear_mods(void) {}
void clear_weak_mods(void) {}
void clear_oneshot_mods(void) {}
void clear_oneshot_locked_mods(void) {}

void add_mods(uint8_t mods) {
    (void)mods;
}

void del_mods(uint8_t mods) {
    (void)mods;
}

void send_keyboard_report(void) {}

void tap_code16(uint16_t keycode) {
    (void)keycode;
}

void register_code16(uint16_t keycode) {
    (void)keycode;
}

void unregister_code16(uint16_t keycode) {
    (void)keycode;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & test_layer_mask(layer)) != 0;
}

void layer_on(uint8_t layer) {
    CHECK(layer < LAYER_COUNT);
    layer_state |= test_layer_mask(layer);
}

void layer_off(uint8_t layer) {
    CHECK(layer < LAYER_COUNT);
    layer_state &= (layer_state_t)~test_layer_mask(layer);
}

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    if (keycode == TEST_MULTI_TAP_KEY) {
        return (key_behavior_view_t){
            .keycode          = keycode,
            .handled          = true,
            .has_multi_tap    = true,
            .tap_hold_term    = 150,
            .longer_hold_term = 350,
            .multi_tap_term   = 120,
            .single =
                {
                    .tap  = TAP_SENDS(LOCK_LAYER(TEST_OTHER_LAYER)),
                    .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(TEST_OTHER_LAYER)),
                },
        };
    }

    return (key_behavior_view_t){
        .keycode = keycode,
    };
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    if (keycode == TEST_MULTI_TAP_KEY && tap_count == 2) {
        return (key_behavior_step_t){
            .tap       = TAP_SENDS(TEST_ALT_ACTION),
            .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(TEST_NUM_LAYER)),
        };
    }

    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    return keycode == TEST_MULTI_TAP_KEY && count < 2;
}

void key_behavior_validate_all(void) {}

bool macro_dispatch(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool noah_synthetic_record_active(void) {
    return false;
}

bool owned_keycode_register(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool owned_keycode_unregister(uint16_t keycode) {
    (void)keycode;
    return false;
}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    (void)keycode;
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    (void)keycode;
}

void noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    (void)keycode;
    (void)pressed;
}

void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count) {
    (void)keycode;
    (void)pressed;
    (void)tap_count;
}

bool noah_qmk_contract_try_play_via_macro(uint16_t keycode) {
    (void)keycode;
    return false;
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
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

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool pd_mode_locked(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

void split_runtime_sync(void) {}

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
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

void held_action_register(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

void held_action_unregister(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

bool held_action_release_owned_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

bool held_modifier_release_owned_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

void held_action_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    (void)key_pos;
    (void)action;
    (void)repeat_hz;
}

void held_action_repeat_tick(void) {}

bool held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
    return false;
}

void key_feedback_pulse_arm(bool long_hold_level) {
    (void)long_hold_level;
}

static void test_double_tap_hold_toggles_num_layer_lock_off_on_second_cycle(void) {
    keypos_t key_pos = test_keypos(4, 2);

    test_reset_state();

    test_run_thumb_like_double_tap_hold_cycle(key_pos);
    CHECK(layer_ownership_is_locked(TEST_NUM_LAYER));
    CHECK(layer_state_cmp(layer_state, TEST_NUM_LAYER));
    CHECK(!layer_ownership_is_locked(TEST_OTHER_LAYER));
    CHECK(!layer_state_cmp(layer_state, TEST_OTHER_LAYER));

    fake_time = (uint16_t)(fake_time + 40);
    test_run_thumb_like_double_tap_hold_cycle(key_pos);
    CHECK(!layer_ownership_is_locked(TEST_NUM_LAYER));
    CHECK(!layer_state_cmp(layer_state, TEST_NUM_LAYER));
    CHECK(!layer_ownership_is_locked(TEST_OTHER_LAYER));
    CHECK(!layer_state_cmp(layer_state, TEST_OTHER_LAYER));
}

static void test_thumb_cycle_release_still_clears_slot_when_layer_change_resolves_to_other_keycode(void) {
    keypos_t             key_pos = test_keypos(4, 2);
    active_key_state_t *slot    = key_runtime_slot_for_position(key_pos);

    test_reset_state();

    test_run_thumb_like_double_tap_hold_cycle_with_release_keycode(key_pos, TEST_FOREIGN_RELEASE_KEY);
    CHECK(layer_ownership_is_locked(TEST_NUM_LAYER));
    CHECK(layer_state_cmp(layer_state, TEST_NUM_LAYER));
    CHECK(slot->owner.keycode == KC_NO);

    fake_time = (uint16_t)(fake_time + 40);
    test_run_thumb_like_double_tap_hold_cycle_with_release_keycode(key_pos, TEST_FOREIGN_RELEASE_KEY);
    CHECK(!layer_ownership_is_locked(TEST_NUM_LAYER));
    CHECK(!layer_state_cmp(layer_state, TEST_NUM_LAYER));
    CHECK(slot->owner.keycode == KC_NO);
}

static void test_double_tap_hold_with_prethreshold_scan_toggles_num_layer_only_once(void) {
    keypos_t key_pos = test_keypos(4, 2);

    test_reset_state();

    test_run_thumb_like_double_tap_hold_cycle_with_intermediate_scan(key_pos, TEST_MULTI_TAP_KEY, 120, 240);
    CHECK(layer_ownership_is_locked(TEST_NUM_LAYER));
    CHECK(layer_state_cmp(layer_state, TEST_NUM_LAYER));

    fake_time = (uint16_t)(fake_time + 40);
    test_run_thumb_like_double_tap_hold_cycle_with_intermediate_scan(key_pos, TEST_MULTI_TAP_KEY, 120, 240);
    CHECK(!layer_ownership_is_locked(TEST_NUM_LAYER));
    CHECK(!layer_state_cmp(layer_state, TEST_NUM_LAYER));
}

int main(void) {
    test_double_tap_hold_toggles_num_layer_lock_off_on_second_cycle();
    test_thumb_cycle_release_still_clears_slot_when_layer_change_resolves_to_other_keycode();
    test_double_tap_hold_with_prethreshold_scan_toggles_num_layer_only_once();

    puts("key_runtime layer-lock integration tests passed");
    return 0;
}
