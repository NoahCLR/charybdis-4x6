#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"

enum {
    TEST_DELAYED_ACTION_A     = 0x0004,
    TEST_DELAYED_ACTION_B     = 0x0005,
    TEST_DELAYED_MACRO_ACTION = QK_MACRO_10,
};

typedef struct {
    uint16_t keycode;
    uint8_t  real;
    uint8_t  weak;
    uint8_t  oneshot;
    uint8_t  oneshot_locked;
    uint8_t  row;
    uint8_t  col;
} tap_call_t;

layer_state_t layer_state;

static uint8_t fake_mods;
static uint8_t fake_weak_mods;
static uint8_t fake_oneshot_mods;
static uint8_t fake_oneshot_locked_mods;

// A fallback hold registers its modifier as a real one, nothing else.
static uint8_t fallback_hold_settle_real_mods;
// The modifier a fallback hold registers through the firmware's own ledger,
// as held_action_register() does for a bare modifier, once: a registered hold
// is never due again.
static uint16_t fallback_hold_settle_ledger_keycode;

static uint8_t    fallback_hold_activation_count;
static tap_call_t tap_call;
static uint8_t    tap_call_count;

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

static void test_reset_stubs(void) {
    fake_mods                                = 0;
    fake_weak_mods                           = 0;
    fake_oneshot_mods                        = 0;
    fake_oneshot_locked_mods                 = 0;
    fallback_hold_settle_real_mods           = 0;
    fallback_hold_settle_ledger_keycode      = KC_NO;
    fallback_hold_activation_count           = 0;
    tap_call                                 = (tap_call_t){0};
    tap_call_count                           = 0;
    keyboard_mod_ownership_reset_for_test();
}

static tap_call_t test_current_tap_call(uint16_t keycode) {
    return (tap_call_t){
        .keycode        = keycode,
        .real           = fake_mods,
        .weak           = fake_weak_mods,
        .oneshot        = fake_oneshot_mods,
        .oneshot_locked = fake_oneshot_locked_mods,
        .row            = MATRIX_ROWS,
        .col            = MATRIX_COLS,
    };
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

void send_keyboard_report(void) {}

bool noah_key_runtime_settle_pending_fallback_hold(keypos_t emitting_key_pos) {
    (void)emitting_key_pos;
    fallback_hold_activation_count++;
    fake_mods |= fallback_hold_settle_real_mods;
    if (fallback_hold_settle_ledger_keycode != KC_NO) {
        keyboard_mod_ownership_register(fallback_hold_settle_ledger_keycode);
        fallback_hold_settle_ledger_keycode = KC_NO;
    }
    return true;
}

bool layer_ownership_is_locked(uint8_t layer) {
    (void)layer;
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

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    (void)key_pos;
    (void)layer;
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    (void)key_pos;
    return true;
}

const pointing_mode_def_t *pointing_mode_lock_action_lookup(uint16_t action) {
    (void)action;
    return NULL;
}

bool pointing_mode_toggle_lock_state(pointing_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool pointing_mode_toggle_lock_state_at(pointing_mode_mask_t mode, keypos_t key_pos) {
    (void)key_pos;
    return pointing_mode_toggle_lock_state(mode);
}

pointing_mode_mask_t pointing_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool is_pointing_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

void noah_action_tap(uint16_t action) {
    tap_call = test_current_tap_call(action);
    tap_call_count++;
}

void noah_action_tap_at(keypos_t key_pos, uint16_t action) {
    tap_call     = test_current_tap_call(action);
    tap_call.row = key_pos.row;
    tap_call.col = key_pos.col;
    tap_call_count++;
}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    (void)keycode;
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    (void)keycode;
}

bool noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    (void)keycode;
    (void)pressed;
    return false;
}

void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count) {
    (void)keycode;
    (void)pressed;
    (void)tap_count;
}

bool owned_keycode_register(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool owned_keycode_unregister(uint16_t keycode) {
    (void)keycode;
    return false;
}

// The ledger is faked here, so a literal tap is recorded as QMK's tap.
void owned_keycode_tap_literal(uint16_t keycode) {
    tap_code16(keycode);
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

bool noah_synthetic_record_active(void) {
    return false;
}

static void test_delayed_action_restores_saved_mod_state_after_emit(void) {
    delayed_action_mods_t mods = {
        .real           = 0x10,
        .weak           = 0x20,
        .oneshot        = 0x40,
        .oneshot_locked = 0x80,
    };

    test_reset_stubs();
    fake_mods                = 0x01;
    fake_weak_mods           = 0x02;
    fake_oneshot_mods        = 0x04;
    fake_oneshot_locked_mods = 0x08;

    dispatch_delayed_action(TEST_DELAYED_ACTION_A, mods);

    CHECK(tap_call_count == 1);
    CHECK(tap_call.keycode == TEST_DELAYED_ACTION_A);
    CHECK(tap_call.real == 0x10);
    CHECK(tap_call.weak == 0x20);
    CHECK(tap_call.oneshot == 0x40);
    CHECK(tap_call.oneshot_locked == 0x80);
    CHECK(fake_mods == 0x01);
    CHECK(fake_weak_mods == 0x02);
    CHECK(fake_oneshot_mods == 0x04);
    CHECK(fake_oneshot_locked_mods == 0x08);
    CHECK(fallback_hold_activation_count == 1);
}

// A fallback hold the tap settles goes out with the tap, and is still held
// when the replay ends: its key is still down.
static void test_delayed_action_settles_fallback_holds_and_keeps_them(void) {
    delayed_action_mods_t mods = {
        .real           = 0x10,
        .weak           = 0x20,
        .oneshot        = 0x40,
        .oneshot_locked = 0x80,
    };

    test_reset_stubs();
    fake_mods                      = 0x01;
    fake_weak_mods                 = 0x02;
    fake_oneshot_mods              = 0x04;
    fake_oneshot_locked_mods       = 0x08;
    fallback_hold_settle_real_mods = 0x02;

    dispatch_delayed_action(TEST_DELAYED_ACTION_B, mods);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(tap_call_count == 1);
    CHECK(tap_call.keycode == TEST_DELAYED_ACTION_B);
    CHECK(tap_call.real == (uint8_t)(mods.real | fallback_hold_settle_real_mods));
    CHECK(tap_call.weak == mods.weak);
    CHECK(tap_call.oneshot == mods.oneshot);
    CHECK(tap_call.oneshot_locked == mods.oneshot_locked);
    CHECK(fake_mods == (uint8_t)(0x01 | fallback_hold_settle_real_mods));
    CHECK(fake_weak_mods == 0x02);
    CHECK(fake_oneshot_mods == 0x04);
    CHECK(fake_oneshot_locked_mods == 0x08);
}

static void test_delayed_macro_runs_with_clean_mod_state(void) {
    delayed_action_mods_t mods = {
        .real           = 0x10,
        .weak           = 0x20,
        .oneshot        = 0x40,
        .oneshot_locked = 0x80,
    };

    test_reset_stubs();
    fake_mods                      = 0x01;
    fake_weak_mods                 = 0x02;
    fake_oneshot_mods              = 0x04;
    fake_oneshot_locked_mods       = 0x08;
    fallback_hold_settle_real_mods = 0x02;

    dispatch_delayed_action(TEST_DELAYED_MACRO_ACTION, mods);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(tap_call_count == 1);
    CHECK(tap_call.keycode == TEST_DELAYED_MACRO_ACTION);
    CHECK(tap_call.real == 0);
    CHECK(tap_call.weak == 0);
    CHECK(tap_call.oneshot == 0);
    CHECK(tap_call.oneshot_locked == 0);
    CHECK(fake_mods == (uint8_t)(0x01 | fallback_hold_settle_real_mods));
    CHECK(fake_weak_mods == 0x02);
    CHECK(fake_oneshot_mods == 0x04);
    CHECK(fake_oneshot_locked_mods == 0x08);
}

// Through the firmware's own modifier ledger: Left Shift with a behaviour row
// is down, counted but not in the report (its press never reached QMK), when
// an earlier key's delayed tap settles its fallback hold. Shift goes out with
// the tap, stays in the report while its key is down, and leaves with its
// release. A delayed macro settles it the same way, and runs without it.
static void test_delayed_tap_keeps_the_modifier_it_settles_held(void) {
    const uint16_t actions[] = {KC_ESC, TEST_DELAYED_MACRO_ACTION};
    keyrecord_t    shift     = {.event = {.key = {.row = 1, .col = 0}, .type = KEY_EVENT, .pressed = true, .time = 1}};

    for (uint8_t index = 0; index < 2u; index++) {
        test_reset_stubs();
        shift.event.pressed = true;
        keyboard_mod_ownership_track_physical_keycode_event(KC_LEFT_SHIFT, &shift);
        CHECK(fake_mods == 0);

        fallback_hold_settle_ledger_keycode = KC_LEFT_SHIFT;
        dispatch_delayed_action_at((keypos_t){.row = 0, .col = 0}, actions[index], (delayed_action_mods_t){0});

        CHECK(tap_call_count == 1);
        CHECK(tap_call.keycode == actions[index]);
        CHECK(tap_call.real == (actions[index] == KC_ESC ? MOD_BIT(KC_LEFT_SHIFT) : 0u));
        CHECK(fake_mods == MOD_BIT(KC_LEFT_SHIFT));

        // Its release: held_action_unregister(), then the physical count.
        keyboard_mod_ownership_unregister(KC_LEFT_SHIFT);
        shift.event.pressed = false;
        keyboard_mod_ownership_track_physical_keycode_event(KC_LEFT_SHIFT, &shift);
        CHECK(fake_mods == 0);
    }
}

static void test_delayed_action_at_preserves_origin_key_pos(void) {
    delayed_action_mods_t mods = {
        .real           = 0x10,
        .weak           = 0x20,
        .oneshot        = 0x40,
        .oneshot_locked = 0x80,
    };
    keypos_t key_pos = {.row = 4, .col = 5};

    test_reset_stubs();

    dispatch_delayed_action_at(key_pos, TEST_DELAYED_ACTION_A, mods);

    CHECK(tap_call_count == 1);
    CHECK(tap_call.keycode == TEST_DELAYED_ACTION_A);
    CHECK(tap_call.row == key_pos.row);
    CHECK(tap_call.col == key_pos.col);
}

int main(void) {
    test_delayed_action_restores_saved_mod_state_after_emit();
    test_delayed_action_settles_fallback_holds_and_keeps_them();
    test_delayed_macro_runs_with_clean_mod_state();
    test_delayed_tap_keeps_the_modifier_it_settles_held();
    test_delayed_action_at_preserves_origin_key_pos();
    puts("delayed_action host tests passed");
    return 0;
}
