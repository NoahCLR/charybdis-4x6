#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_runtime_integration_harness.h"
#include "users/noah/noah_runtime.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/compat/qmk_mod_contract.h"
#include "users/noah/lib/state/modifiers/keyboard_mod_policy.h"
#include "users/noah/lib/key/behavior/key_behavior_lookup.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/key/runtime/projection/projection.h"
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
    {.keycode = TEST_SHIFTED_SYMBOL_KEY, .single = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_1))}}, {.keycode = TEST_SHIFT_ENTER_KEY, .single = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_ENT))}}, {.keycode = TEST_HANDLED_SHIFT_KEY, .single = {.tap = TAP_SENDS(KC_CAPS)}}, {.keycode = TEST_MANAGED_SHIFT_KEY, .single = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LEFT_SHIFT)}}, {.keycode = TEST_SHIFTED_PLAIN_KEY, .single = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_A))}},
};

// A row with an authored tap and the MT source's intrinsic modifier hold. This
// harness's lookup handles only keys with a row, so the row also stands in for
// the default every MT() gets without one (D-F27): QMK's tapping decides none.
static test_behavior_row_t test_authored_mod_tap;

static uint16_t fake_time;
static uint8_t  fake_mods;
static uint8_t  fake_weak_mods;
static uint8_t  fake_oneshot_mods;
static uint8_t  fake_oneshot_locked_mods;

static uint8_t send_keyboard_report_count;
static uint8_t report_mods;
static uint8_t report_required_mods;
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
    if (test_authored_mod_tap.keycode != KC_NO && keycode == test_authored_mod_tap.keycode) {
        return &test_authored_mod_tap;
    }
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
    report_mods                = 0;
    report_required_mods        = 0;
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
    CHECK((get_mods() & report_required_mods) == report_required_mods);
    report_mods = get_mods() | get_weak_mods() | get_oneshot_mods();
    // QMK consumes unlocked one-shot modifiers when a keyboard report contains
    // a basic key, even when that report does not add another usage edge.
    if (get_oneshot_mods()) {
        for (uint16_t code = KC_A; code <= KC_EXSEL; code++) {
            if (report_key_depth[code] > 0) {
                clear_oneshot_mods();
                break;
            }
        }
    }
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

void pointer_layer_policy_sync_layer_ownership_anchor(void) {}

void pointer_layer_policy_settle_record(void) {}

void pointer_layer_policy_take_back_qmk_toggle(uint16_t keycode, const keyrecord_t *record) {
    (void)keycode;
    (void)record;
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

// The modifier bits a concurrent key would carry while the mode masks `mods`.
static uint8_t test_mods_for_concurrent_key(uint8_t mods) {
    uint8_t mask = keyboard_mod_policy_managed_only_mask(mods);
    uint8_t seen;

    (void)keyboard_mod_policy_begin_real_mod_mask(mask);
    seen = get_mods();
    keyboard_mod_policy_end_real_mod_mask(mask);
    return seen;
}

static void test_assert_mods_balanced(void) {
    keyboard_mod_ownership_debug_snapshot_t snapshot = {0};

    keyboard_mod_ownership_debug_snapshot(&snapshot);
    for (uint8_t index = 0; index < KEYBOARD_MOD_OWNERSHIP_MOD_COUNT; index++) {
        CHECK(snapshot.physical_refcounts[index] == 0);
        CHECK(snapshot.managed_refcounts[index] == 0);
        CHECK(snapshot.report_refcounts[index] == 0);
    }
    CHECK(get_mods() == 0);
}

static uint8_t test_managed_mod_refcount_total(void) {
    keyboard_mod_ownership_debug_snapshot_t snapshot = {0};
    uint8_t                                 total    = 0;

    keyboard_mod_ownership_debug_snapshot(&snapshot);
    for (uint8_t index = 0; index < KEYBOARD_MOD_OWNERSHIP_MOD_COUNT; index++) {
        total = (uint8_t)(total + snapshot.managed_refcounts[index]);
    }
    return total;
}

// A tapped mod-tap never holds its modifier: the mode's Cmd is its only
// owner, before the tap and after it, and stays masked. Held past its hold
// time, the same key does take Cmd, so the count tells a tap from a hold.
// (A held mod-tap is masked under such a mode too, an open issue, so masking
// alone cannot tell them apart.)
static void test_tapped_mod_tap_is_not_a_held_modifier(void) {
    const uint16_t gui_tap = MT(MOD_LGUI, KC_A);
    keypos_t       mt_pos  = test_keypos(4, 0);
    uint8_t        gui     = MOD_BIT(KC_LEFT_GUI);

    test_authored_mod_tap = (test_behavior_row_t){
        .keycode = gui_tap,
        .single  = {.tap = TAP_SENDS(KC_A), .hold = PRESS_AND_HOLD_UNTIL_RELEASE((uint16_t)(MOD_LGUI << 8u))},
    };
    for (uint8_t held = 0; held < 2u; held++) {
        test_reset_state();
        keyboard_mod_ownership_register(KC_LEFT_GUI);
        CHECK(test_managed_mod_refcount_total() == 1u);
        CHECK(!key_runtime_integration_process_record(gui_tap, mt_pos, true));
        CHECK(test_managed_mod_refcount_total() == 1u);
        CHECK(test_mods_for_concurrent_key(gui) == 0);
        if (held) {
            key_runtime_integration_advance(&fake_time, 400u);
            key_runtime_integration_scan();
            CHECK(test_managed_mod_refcount_total() == 2u);
        }
        CHECK(!key_runtime_integration_process_record(gui_tap, mt_pos, false));
        CHECK(test_managed_mod_refcount_total() == 1u);
        CHECK(test_mods_for_concurrent_key(gui) == 0);
        keyboard_mod_ownership_unregister(KC_LEFT_GUI);
        key_runtime_integration_advance(&fake_time, 400u);
        key_runtime_integration_scan();
        test_assert_mods_balanced();
    }
    test_authored_mod_tap = (test_behavior_row_t){0};
}

// Compare report masks from the reducer with the real applied ownership ledger,
// not with a second expected-value decoder. All 15 subsets on each side are
// supported by QMK; its five-bit encoding cannot express mixed-side holds.
static void test_assert_mod_tap_projection(uint8_t expected) {
    projection_snapshot_t applied = key_runtime_core_projection_snapshot_capture();
    const key_runtime_core_shadow_projection_t *shadow = &key_runtime_core_state()->shadow_projection;

    CHECK(applied.keyboard_mod_state.real == expected);
    CHECK(applied.keyboard_managed_mod_mask == expected);
    CHECK(applied.keyboard_physical_mod_mask == 0u);
    if (shadow->keyboard_mod_state.real != applied.keyboard_mod_state.real) {
        fprintf(stderr, "MT shadow/applied disagreement: shadow=0x%02x applied=0x%02x expected=0x%02x\n",
                shadow->keyboard_mod_state.real, applied.keyboard_mod_state.real, expected);
    }
    CHECK(shadow->keyboard_mod_state.real == applied.keyboard_mod_state.real);
    CHECK(shadow->keyboard_managed_mod_mask == applied.keyboard_managed_mod_mask);
}

static void test_assert_mod_tap_quiescent(void) {
    test_assert_mod_tap_projection(0u);
    test_assert_mods_balanced();
    CHECK(key_runtime_core_state()->press_token_count == 0u);
    CHECK(key_runtime_core_state()->lease_count == 0u);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0u);
    CHECK(noah_runtime_debug_deferred_release_count() == 0u);
    CHECK(key_runtime_core_state()->shadow_projection.keyboard_physical_mod_mask == 0u);
}

static void test_mod_tap_shadow_matches_applied_ownership(void) {
    const keypos_t pos = {4, 3};

    for (uint8_t side = 0; side < 2u; side++) {
        for (uint8_t subset = 1u; subset < 16u; subset++) {
            const uint8_t encoded = (uint8_t)(subset | (side ? 0x10u : 0u));
            const uint8_t expected = (uint8_t)(side ? subset << 4u : subset);
            const uint16_t code = MT(encoded, KC_Q);

            // Authored hold is applied through the real literal action ledger.
            test_authored_mod_tap = (test_behavior_row_t){
                .keycode = code,
                .single = {.tap = TAP_SENDS(KC_Q), .hold = PRESS_AND_HOLD_UNTIL_RELEASE((uint16_t)(encoded << 8u))},
            };
            test_reset_state();
            CHECK(!key_runtime_integration_process_record(code, pos, true));
            test_assert_mod_tap_projection(0u);
            key_runtime_integration_advance(&fake_time, TEST_HOLD_TERM + 1u);
            key_runtime_integration_scan();
            test_assert_mod_tap_projection(expected);
            CHECK(!key_runtime_integration_process_record(code, pos, false));
            key_runtime_integration_advance(&fake_time, 400u);
            key_runtime_integration_scan();
            test_assert_mod_tap_quiescent();

            test_reset_state();
            CHECK(!key_runtime_integration_process_record(code, pos, true));
            test_assert_mod_tap_projection(0u);
            CHECK(!key_runtime_integration_process_record(code, pos, false));
            key_runtime_integration_advance(&fake_time, 400u);
            key_runtime_integration_scan();
            test_assert_mod_tap_quiescent();
            test_authored_mod_tap = (test_behavior_row_t){0};
        }
    }

    // A row whose hold is not the MT's own modifiers holds none of them: the
    // shadow follows what the hold holds, not the keycode family (D-F27).
    const uint16_t code = MT(MOD_LCTL, KC_Q);
    test_authored_mod_tap = (test_behavior_row_t){
        .keycode = code,
        .single = {.tap = TAP_SENDS(KC_Q), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_B)},
    };
    test_reset_state();
    CHECK(!key_runtime_integration_process_record(code, pos, true));
    key_runtime_integration_advance(&fake_time, TEST_HOLD_TERM + 1u);
    key_runtime_integration_scan();
    CHECK(noah_runtime_debug_slot_held_action_keycode(pos) == KC_B);
    test_assert_mod_tap_projection(0u);
    CHECK(!key_runtime_integration_process_record(code, pos, false));
    key_runtime_integration_advance(&fake_time, 400u);
    key_runtime_integration_scan();
    test_assert_mod_tap_quiescent();
    test_authored_mod_tap = (test_behavior_row_t){0};
}

// Native ACT_LMODS/ACT_RMODS with KC_NO or a modifier uses add_mods/del_mods,
// not register_mods. Run that default action between the real process and
// finalize hooks; a consumed release must never send a transient missing mod.
static void test_native_modifier_event(uint16_t code, keypos_t pos, bool pressed, uint8_t mods) {
    keyrecord_t record = {.event = {.type = KEY_EVENT, .key = pos, .pressed = pressed, .time = fake_time}};
    CHECK(noah_pre_process_record_user(code, &record));
    if (noah_process_record_user(code, &record)) {
        if (pressed) add_mods(mods);
        else del_mods(mods);
        send_keyboard_report();
        noah_post_process_record_user(code, &record);
    } else {
        noah_process_record_user_finalize(code, &record, false);
    }
}

static void test_native_modifier_combination(uint16_t code, uint8_t mods) {
    const keypos_t pos = {4, 4};
    uint8_t shared = (uint8_t)(mods & (uint8_t)(0u - mods));
    for (uint8_t order = 0; order < 4u; order++) {
        test_reset_state();
        if (order & 1u) keyboard_mod_ownership_register_mods(shared);
        test_native_modifier_event(code, pos, true, mods);
        if (!(order & 1u)) keyboard_mod_ownership_register_mods(shared);
        CHECK(get_mods() == mods);
        CHECK(keyboard_mod_policy_managed_only_mask(shared) == 0);
        CHECK(key_runtime_core_projection_snapshot_capture().keyboard_physical_mod_mask == mods);
        CHECK(key_runtime_core_state()->shadow_projection.keyboard_physical_mod_mask == mods);
        report_required_mods = shared;
        if (order & 2u) {
            test_native_modifier_event(code, pos, false, mods);
            CHECK(get_mods() == shared); // unshared parts must leave immediately
            CHECK(keyboard_mod_policy_managed_only_mask(shared) == shared);
            report_required_mods = 0;
            keyboard_mod_ownership_unregister_mods(shared);
        } else {
            keyboard_mod_ownership_unregister_mods(shared);
            CHECK(get_mods() == mods); // the native key still holds every part
            report_required_mods = 0;
            test_native_modifier_event(code, pos, false, mods);
        }
        test_assert_mods_balanced();
        CHECK(key_runtime_core_state()->press_token_count == 0u);
        CHECK(key_runtime_core_state()->lease_count == 0u);
        CHECK(key_runtime_core_state()->shadow_projection.keyboard_physical_mod_mask == 0);
    }
}

static void test_native_modifier_combinations_preserve_every_owner(void) {
    for (uint8_t side = 0; side < 2u; side++) {
        for (uint8_t subset = 1; subset < 16u; subset++) {
            uint8_t encoded = (uint8_t)(subset | (side ? 0x10u : 0u));
            test_native_modifier_combination((uint16_t)encoded << 8u, side ? (uint8_t)(subset << 4u) : subset);
        }
    }
    test_native_modifier_combination(S(KC_LEFT_CTRL), MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_CTRL));
    test_native_modifier_combination(RSFT(KC_LEFT_CTRL), MOD_BIT(KC_RIGHT_SHIFT) | MOD_BIT(KC_LEFT_CTRL));
    test_native_modifier_combination(C(KC_LEFT_CTRL), MOD_BIT(KC_LEFT_CTRL));

    // A plain physical modifier overlaps a combination too, without a managed
    // owner. Neither release may clear the other physical owner's component.
    for (uint8_t order = 0; order < 2u; order++) {
        const keypos_t plain = {4, 3}, combo = {4, 4};
        uint8_t ctrl = MOD_BIT(KC_LEFT_CTRL), meh = ctrl | MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_ALT);
        test_reset_state();
        test_native_modifier_event(KC_MEH, combo, true, meh);
        test_native_modifier_event(KC_LEFT_CTRL, plain, true, ctrl);
        report_required_mods = ctrl;
        if (order) {
            test_native_modifier_event(KC_LEFT_CTRL, plain, false, ctrl);
            CHECK(get_mods() == meh);
            report_required_mods = 0;
            test_native_modifier_event(KC_MEH, combo, false, meh);
        } else {
            test_native_modifier_event(KC_MEH, combo, false, meh);
            CHECK(get_mods() == ctrl);
            report_required_mods = 0;
            test_native_modifier_event(KC_LEFT_CTRL, plain, false, ctrl);
        }
        test_assert_mods_balanced();
    }
}

static void test_modified_ordinary_key_keeps_native_weak_mod_path(void) {
    const keypos_t pos = {4, 4};
    const uint16_t code = S(KC_A);
    test_reset_state();
    keyboard_mod_ownership_register(KC_LEFT_SHIFT);
    CHECK(key_runtime_integration_process_record(code, pos, true));
    CHECK(keyboard_mod_policy_managed_only_mask(MOD_BIT(KC_LEFT_SHIFT)) == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(key_runtime_core_state()->shadow_projection.keyboard_physical_mod_mask == 0);
    CHECK(key_runtime_integration_process_record(code, pos, false));
    keyboard_mod_ownership_unregister(KC_LEFT_SHIFT);
    test_assert_mods_balanced();
}

static void test_consumed_modifier_combination_does_not_own_report_parts(void) {
    const keypos_t pos = {4, 4};
    const uint16_t code = KC_HYPR;
    const uint8_t mods = MOD_BIT(KC_LEFT_CTRL) | MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_GUI);
    test_authored_mod_tap = (test_behavior_row_t){.keycode = code, .single = {.tap = TAP_SENDS(KC_Q)}};
    test_reset_state();
    CHECK(!key_runtime_integration_process_record(code, pos, true));
    CHECK(test_report_mod_refcount_total() == 0);
    keyboard_mod_ownership_register_mods(mods);
    keyboard_mod_ownership_unregister_mods(mods);
    CHECK(get_mods() == 0); // a consumed physical press cannot veto teardown
    CHECK(!key_runtime_integration_process_record(code, pos, false));
    key_runtime_integration_advance(&fake_time, 400u);
    key_runtime_integration_scan();
    test_assert_mods_balanced();
    test_authored_mod_tap = (test_behavior_row_t){0};
}

// A duplicate basic edge must not suppress a modified native key's own weak
// modifier transitions or forget the physical owner that remains held.
static void test_overlapping_native_shortcut_keeps_weak_mods_and_basic_owner(void) {
    const uint16_t shortcuts[] = {S(KC_A), RSFT(KC_A)};
    const uint8_t masks[] = {MOD_BIT(KC_LEFT_SHIFT), MOD_BIT(KC_RIGHT_SHIFT)};
    for (uint8_t i = 0; i < ARRAY_SIZE(shortcuts); i++) {
        for (uint8_t physical_releases_first = 0; physical_releases_first < 2; physical_releases_first++) {
            test_reset_state();
            owned_keycode_lease_t hold = {0};
            CHECK(owned_keycode_acquire(KC_A, &hold));
            CHECK(!key_runtime_integration_process_record(shortcuts[i], test_keypos(5, 1), true));
            CHECK(get_weak_mods() == masks[i]);
            CHECK(test_owned_snapshot(KC_A).physical_count == 1u);
            CHECK(report_key_depth[KC_A] == 1);
            if (physical_releases_first) {
                CHECK(!key_runtime_integration_process_record(shortcuts[i], test_keypos(5, 1), false));
                CHECK(get_weak_mods() == 0u && report_key_depth[KC_A] == 1);
                CHECK(owned_keycode_release(&hold));
            } else {
                CHECK(owned_keycode_release(&hold));
                CHECK(get_weak_mods() == masks[i] && report_key_depth[KC_A] == 1);
                CHECK(key_runtime_integration_process_record(shortcuts[i], test_keypos(5, 1), false));
                // QMK performs the last owner's normal default action.
                unregister_code(KC_A);
                set_weak_mods(0u);
            }
            CHECK(report_key_depth[KC_A] == 0);
            test_assert_ledgers_balanced(KC_A);
        }
    }
}

// Model the pinned QMK action_exec/ACT_LMODS boundary, including the weak-mod
// clear before every press. The default basic edge runs only if userspace
// admits it, and finalize follows that edge as it does in QMK.
static bool test_native_keyboard_report_event(uint16_t code, keypos_t pos, bool pressed) {
    keyrecord_t record = {.event = {.type = KEY_EVENT, .key = pos, .pressed = pressed, .time = fake_time}};
    uint8_t basic = IS_QK_MODS(code) ? QK_MODS_GET_BASIC_KEYCODE(code) : (uint8_t)code;
    uint8_t mods = IS_QK_MODS(code) ? noah_qmk_mods_to_report_mask(QK_MODS_GET_MODS(code)) : 0u;
    if (pressed) clear_weak_mods();
    CHECK(noah_pre_process_record_user(code, &record));
    bool keep_processing = noah_process_record_user(code, &record);
    if (keep_processing) {
        if (pressed) {
            if (mods) {
                set_weak_mods(get_weak_mods() | mods);
                send_keyboard_report();
            }
            register_code(basic);
            send_keyboard_report();
        } else {
            unregister_code(basic);
            send_keyboard_report();
            if (mods) {
                set_weak_mods(get_weak_mods() & (uint8_t)~mods);
                send_keyboard_report();
            }
        }
        noah_post_process_record_user(code, &record);
    } else {
        noah_process_record_user_finalize(code, &record, false);
    }
    return keep_processing;
}

static void test_plain_native_overlap_reports_cleared_weak_mods(void) {
    const uint16_t shortcuts[] = {S(KC_A), RSFT(KC_A)};
    const uint8_t masks[] = {MOD_BIT(KC_LEFT_SHIFT), MOD_BIT(KC_RIGHT_SHIFT)};
    const keypos_t shifted = {5, 1}, plain = {5, 2};
    for (uint8_t i = 0; i < ARRAY_SIZE(shortcuts); i++) {
        for (uint8_t plain_releases_first = 0; plain_releases_first < 2; plain_releases_first++) {
            test_reset_state();
            CHECK(test_native_keyboard_report_event(shortcuts[i], shifted, true));
            CHECK(report_mods == masks[i] && report_key_depth[KC_A] == 1);
            uint8_t reports_before_overlap = send_keyboard_report_count;
            CHECK(!test_native_keyboard_report_event(KC_A, plain, true));
            CHECK(send_keyboard_report_count > reports_before_overlap);
            CHECK(report_mods == 0 && get_weak_mods() == 0);
            CHECK(report_key_depth[KC_A] == 1);
            CHECK(test_owned_snapshot(KC_A).physical_count == 2);
            if (plain_releases_first) {
                CHECK(!test_native_keyboard_report_event(KC_A, plain, false));
                CHECK(report_key_depth[KC_A] == 1);
                CHECK(test_native_keyboard_report_event(shortcuts[i], shifted, false));
            } else {
                CHECK(!test_native_keyboard_report_event(shortcuts[i], shifted, false));
                CHECK(report_key_depth[KC_A] == 1);
                CHECK(test_native_keyboard_report_event(KC_A, plain, false));
            }
            CHECK(report_mods == 0 && report_key_depth[KC_A] == 0);
            test_assert_ledgers_balanced(KC_A);
        }
    }
}

static void test_plain_native_overlap_consumes_oneshot_mods(void) {
    const keypos_t first = {5, 1}, second = {5, 2};
    test_reset_state();
    CHECK(test_native_keyboard_report_event(KC_A, first, true));
    set_oneshot_mods(MOD_BIT(KC_LEFT_SHIFT));
    CHECK(!test_native_keyboard_report_event(KC_A, second, true));
    CHECK(report_mods == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(get_oneshot_mods() == 0);
    CHECK(report_key_depth[KC_A] == 1);
    CHECK(!test_native_keyboard_report_event(KC_A, second, false));
    CHECK(report_mods == 0 && report_key_depth[KC_A] == 1);
    CHECK(test_native_keyboard_report_event(KC_A, first, false));
    CHECK(report_key_depth[KC_A] == 0);
    test_assert_ledgers_balanced(KC_A);
}

int main(void) {
    test_plain_native_overlap_reports_cleared_weak_mods();
    test_plain_native_overlap_consumes_oneshot_mods();
    test_overlapping_native_shortcut_keeps_weak_mods_and_basic_owner();
    test_native_modifier_combinations_preserve_every_owner();
    test_modified_ordinary_key_keeps_native_weak_mod_path();
    test_consumed_modifier_combination_does_not_own_report_parts();
    test_mod_tap_shadow_matches_applied_ownership();
    test_handled_same_basic_hold_registers_base_key(TEST_SHIFTED_SYMBOL_KEY, KC_1);
    test_handled_same_basic_hold_registers_base_key(TEST_SHIFT_ENTER_KEY, KC_ENT);
    test_default_processed_key_still_blocks_managed_double_register();
    test_consumed_handled_modifier_does_not_block_managed_teardown();
    test_tapped_mod_tap_is_not_a_held_modifier();

    puts("key_runtime physical-ownership integration tests passed");
    return 0;
}

// Participation allows every press in this harness (no stored policy), so a
// row-less view is never asked for beyond linking.
key_behavior_view_t key_behavior_lookup_without_row(uint16_t keycode) {
    return key_behavior_lookup(keycode);
}
