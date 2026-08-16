#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_runtime_integration_harness.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/key/behavior/key_behavior_lookup.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "users/noah/lib/state/diagnostics/runtime_debug.h"
#include "users/noah/lib/state/shared/runtime_reset.h"

// Physical ownership means "QMK's default handler registered this key and has
// not released it yet". These tests drive the real pre-process/process/finalize
// seam against the real ownership ledgers so a consumed handled event cannot
// claim report ownership it never took.

enum {
    TEST_HOLD_TERM = 150,
};

enum {
    TEST_SHIFTED_SYMBOL_KEY = KC_1,
    TEST_SHIFT_ENTER_KEY    = KC_ENT,
    TEST_HANDLED_SHIFT_KEY  = KC_LEFT_SHIFT,
    TEST_MANAGED_SHIFT_KEY  = SAFE_RANGE + 0x71,
    TEST_PLAIN_KEY          = KC_A,
    TEST_SHIFTED_PLAIN_KEY  = SAFE_RANGE + 0x72,
};

typedef struct {
    uint16_t            keycode;
    key_behavior_step_t single;
} test_behavior_row_t;

static const test_behavior_row_t test_behavior_rows[] = {
    {.keycode = TEST_SHIFTED_SYMBOL_KEY, .single = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_1))}},
    {.keycode = TEST_SHIFT_ENTER_KEY, .single = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_ENT))}},
    {.keycode = TEST_HANDLED_SHIFT_KEY, .single = {.tap = TAP_SENDS(KC_CAPS)}},
    {.keycode = TEST_MANAGED_SHIFT_KEY, .single = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LEFT_SHIFT)}},
    {.keycode = TEST_SHIFTED_PLAIN_KEY, .single = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_A))}},
};

static uint16_t fake_time;
static uint8_t  fake_mods;
static uint8_t  fake_weak_mods;
static uint8_t  fake_oneshot_mods;
static uint8_t  fake_oneshot_locked_mods;

static uint8_t send_keyboard_report_count;
static uint8_t emitted_action_count;
static int8_t  report_key_depth[UINT8_MAX + 1];

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

static const test_behavior_row_t *test_behavior_row(uint16_t keycode) {
    for (uint8_t index = 0; index < ARRAY_SIZE(test_behavior_rows); index++) {
        if (test_behavior_rows[index].keycode == keycode) {
            return &test_behavior_rows[index];
        }
    }

    return NULL;
}

static uint8_t test_report_mod_refcount_total(void) {
    keyboard_mod_ownership_debug_snapshot_t snapshot = {0};
    uint8_t                                 total    = 0;

    keyboard_mod_ownership_debug_snapshot(&snapshot);
    for (uint8_t index = 0; index < KEYBOARD_MOD_OWNERSHIP_MOD_COUNT; index++) {
        total = (uint8_t)(total + snapshot.report_refcounts[index]);
    }

    return total;
}

static owned_keycode_debug_snapshot_t test_owned_snapshot(uint8_t basic) {
    owned_keycode_debug_snapshot_t snapshot = {0};

    owned_keycode_debug_snapshot(basic, &snapshot);
    return snapshot;
}

static void test_reset_state(void) {
    fake_time                  = 1000;
    fake_mods                  = 0;
    fake_weak_mods             = 0;
    fake_oneshot_mods          = 0;
    fake_oneshot_locked_mods   = 0;
    send_keyboard_report_count = 0;
    emitted_action_count       = 0;
    for (uint16_t index = 0; index <= UINT8_MAX; index++) {
        report_key_depth[index] = 0;
    }
    noah_runtime_reset_for_test();
    send_keyboard_report_count = 0;
}

static void test_assert_ledgers_balanced(uint8_t basic) {
    owned_keycode_debug_snapshot_t snapshot = test_owned_snapshot(basic);

    CHECK(snapshot.physical_count == 0);
    CHECK(snapshot.managed_count == 0);
    CHECK(snapshot.underflow_count == 0);
    CHECK(snapshot.saturation_count == 0);
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
    report_key_depth[keycode]++;
}

void unregister_code(uint8_t keycode) {
    report_key_depth[keycode]--;
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
    const test_behavior_row_t *row = test_behavior_row(keycode);

    if (!row) {
        return (key_behavior_view_t){
            .keycode = keycode,
            .handled = false,
        };
    }

    return (key_behavior_view_t){
        .keycode          = keycode,
        .handled          = true,
        .tap_hold_term    = TEST_HOLD_TERM,
        .longer_hold_term = 350,
        .multi_tap_term   = 120,
        .single           = row->single,
    };
}

key_behavior_step_t key_behavior_view_step(const key_behavior_view_t *behavior, uint8_t tap_count) {
    if (!behavior || tap_count > 1) {
        return key_behavior_step_none();
    }

    return behavior->single;
}

bool key_behavior_view_has_more_taps(const key_behavior_view_t *behavior, uint8_t count) {
    (void)behavior;
    (void)count;
    return false;
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    const test_behavior_row_t *row = test_behavior_row(keycode);

    if (!row || tap_count > 1) {
        return key_behavior_step_none();
    }

    return row->single;
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    (void)keycode;
    (void)count;
    return false;
}

bool key_behavior_future_tap_path_has_foreign_pd_mode(uint16_t keycode, uint8_t count, pd_mode_mask_t base_mode) {
    (void)keycode;
    (void)count;
    (void)base_mode;
    return false;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    (void)action;
    (void)policy;
    emitted_action_count++;
}

void noah_emit_action_tap_at(keypos_t key_pos, uint16_t action, noah_emit_policy_t policy) {
    (void)key_pos;
    noah_emit_action_tap(action, policy);
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    (void)action;
    (void)mods;
}

void dispatch_delayed_action_at(keypos_t key_pos, uint16_t action, delayed_action_mods_t mods) {
    (void)key_pos;
    dispatch_delayed_action(action, mods);
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

bool pd_mode_set_lock_state(pd_mode_mask_t mode, bool locked) {
    (void)mode;
    (void)locked;
    return false;
}

bool pd_mode_set_lock_state_at(pd_mode_mask_t mode, bool locked, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_set_lock_state(mode, locked);
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

static void test_activate_single_hold(uint16_t keycode, keypos_t key_pos) {
    CHECK(!key_runtime_integration_process_record(keycode, key_pos, true));

    key_runtime_integration_advance(&fake_time, TEST_HOLD_TERM + 1);
    key_runtime_integration_scan();
}

// A handled hold whose action carries the same basic usage as its source key
// must still deliver that usage. The consumed press never reached QMK's default
// handler, so it owns nothing in the report.
static void test_handled_same_basic_hold_registers_base_key(uint16_t source_keycode, uint8_t basic) {
    keypos_t source_pos = test_keypos(1, 1);

    test_reset_state();

    CHECK(!key_runtime_integration_process_record(source_keycode, source_pos, true));
    CHECK(test_owned_snapshot(basic).physical_count == 0);
    CHECK(report_key_depth[basic] == 0);

    key_runtime_integration_advance(&fake_time, TEST_HOLD_TERM + 1);
    key_runtime_integration_scan();

    CHECK(report_key_depth[basic] == 1);
    CHECK((get_mods() & MOD_BIT(KC_LEFT_SHIFT)) != 0);
    CHECK(test_owned_snapshot(basic).managed_count == 1);

    CHECK(!key_runtime_integration_process_record(source_keycode, source_pos, false));

    CHECK(report_key_depth[basic] == 0);
    CHECK((get_mods() & MOD_BIT(KC_LEFT_SHIFT)) == 0);
    test_assert_ledgers_balanced(basic);
}

// The original aggregate-ownership contract still has to hold: a key QMK really
// did register must stop a managed owner of the same usage from registering it
// a second time, or releasing it out from under the physical key.
static void test_default_processed_key_still_blocks_managed_double_register(void) {
    keypos_t plain_pos = test_keypos(2, 1);
    keypos_t hold_pos  = test_keypos(2, 2);

    test_reset_state();

    CHECK(key_runtime_integration_process_record(TEST_PLAIN_KEY, plain_pos, true));
    CHECK(test_owned_snapshot(KC_A).physical_count == 1);

    test_activate_single_hold(TEST_SHIFTED_PLAIN_KEY, hold_pos);

    CHECK(report_key_depth[KC_A] == 0);
    CHECK((get_mods() & MOD_BIT(KC_LEFT_SHIFT)) != 0);
    CHECK(test_owned_snapshot(KC_A).managed_count == 1);

    CHECK(!key_runtime_integration_process_record(TEST_SHIFTED_PLAIN_KEY, hold_pos, false));

    CHECK(report_key_depth[KC_A] == 0);
    CHECK(test_owned_snapshot(KC_A).physical_count == 1);

    CHECK(key_runtime_integration_process_record(TEST_PLAIN_KEY, plain_pos, false));

    test_assert_ledgers_balanced(KC_A);
}

// A consumed handled modifier press is physically down but owns nothing in the
// report. If it were credited as a report owner, the last managed owner's
// teardown would skip its del_mods() and leave the modifier asserted.
static void test_consumed_handled_modifier_does_not_block_managed_teardown(void) {
    keypos_t handled_pos = test_keypos(3, 1);
    keypos_t managed_pos = test_keypos(3, 2);

    test_reset_state();

    CHECK(!key_runtime_integration_process_record(TEST_HANDLED_SHIFT_KEY, handled_pos, true));
    CHECK(test_report_mod_refcount_total() == 0);

    key_runtime_integration_advance(&fake_time, TEST_HOLD_TERM + 1);
    key_runtime_integration_scan();
    CHECK((get_mods() & MOD_BIT(KC_LEFT_SHIFT)) != 0);

    test_activate_single_hold(TEST_MANAGED_SHIFT_KEY, managed_pos);
    CHECK((get_mods() & MOD_BIT(KC_LEFT_SHIFT)) != 0);

    CHECK(!key_runtime_integration_process_record(TEST_MANAGED_SHIFT_KEY, managed_pos, false));
    CHECK((get_mods() & MOD_BIT(KC_LEFT_SHIFT)) != 0);

    CHECK(!key_runtime_integration_process_record(TEST_HANDLED_SHIFT_KEY, handled_pos, false));
    CHECK((get_mods() & MOD_BIT(KC_LEFT_SHIFT)) == 0);
    CHECK(test_report_mod_refcount_total() == 0);
}

int main(void) {
    test_handled_same_basic_hold_registers_base_key(TEST_SHIFTED_SYMBOL_KEY, KC_1);
    test_handled_same_basic_hold_registers_base_key(TEST_SHIFT_ENTER_KEY, KC_ENT);
    test_default_processed_key_still_blocks_managed_double_register();
    test_consumed_handled_modifier_does_not_block_managed_teardown();

    puts("key_runtime physical-ownership integration tests passed");
    return 0;
}
