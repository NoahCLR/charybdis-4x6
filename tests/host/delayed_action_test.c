#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/key/runtime/delayed_action.h"

enum {
    TEST_DELAYED_ACTION_A = 0x0004,
    TEST_DELAYED_ACTION_B = 0x0005,
};

typedef struct {
    uint16_t keycode;
    uint8_t  real;
    uint8_t  weak;
    uint8_t  oneshot;
    uint8_t  oneshot_locked;
} tap_call_t;

static uint8_t fake_mods;
static uint8_t fake_weak_mods;
static uint8_t fake_oneshot_mods;
static uint8_t fake_oneshot_locked_mods;

static uint8_t fallback_hold_settle_real_mods;
static uint8_t fallback_hold_settle_weak_mods;
static uint8_t fallback_hold_settle_oneshot_mods;
static uint8_t fallback_hold_settle_oneshot_locked_mods;

static uint8_t fallback_hold_activation_count;
static tap_call_t tap_call;
static uint8_t tap_call_count;

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
    fake_mods                         = 0;
    fake_weak_mods                    = 0;
    fake_oneshot_mods                 = 0;
    fake_oneshot_locked_mods          = 0;
    fallback_hold_settle_real_mods    = 0;
    fallback_hold_settle_weak_mods    = 0;
    fallback_hold_settle_oneshot_mods = 0;
    fallback_hold_settle_oneshot_locked_mods = 0;
    fallback_hold_activation_count    = 0;
    tap_call                          = (tap_call_t){0};
    tap_call_count                    = 0;
}

static tap_call_t test_current_tap_call(uint16_t keycode) {
    return (tap_call_t){
        .keycode        = keycode,
        .real           = fake_mods,
        .weak           = fake_weak_mods,
        .oneshot        = fake_oneshot_mods,
        .oneshot_locked = fake_oneshot_locked_mods,
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

void send_keyboard_report(void) {}

bool noah_key_runtime_settle_pending_fallback_hold(void) {
    fallback_hold_activation_count++;
    fake_mods |= fallback_hold_settle_real_mods;
    fake_weak_mods |= fallback_hold_settle_weak_mods;
    fake_oneshot_mods |= fallback_hold_settle_oneshot_mods;
    fake_oneshot_locked_mods |= fallback_hold_settle_oneshot_locked_mods;
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

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    (void)key_pos;
    (void)layer;
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    (void)key_pos;
    return true;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    (void)action;
    return NULL;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

void noah_action_tap(uint16_t action) {
    tap_call = test_current_tap_call(action);
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

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
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

static void test_delayed_action_settles_fallback_holds_inside_replay_window(void) {
    delayed_action_mods_t mods = {
        .real           = 0x10,
        .weak           = 0x20,
        .oneshot        = 0x40,
        .oneshot_locked = 0x80,
    };

    test_reset_stubs();
    fake_mods                         = 0x01;
    fake_weak_mods                    = 0x02;
    fake_oneshot_mods                 = 0x04;
    fake_oneshot_locked_mods          = 0x08;
    fallback_hold_settle_real_mods    = 0x03;
    fallback_hold_settle_weak_mods    = 0x05;
    fallback_hold_settle_oneshot_mods = 0x06;
    fallback_hold_settle_oneshot_locked_mods = 0x07;

    dispatch_delayed_action(TEST_DELAYED_ACTION_B, mods);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(tap_call_count == 1);
    CHECK(tap_call.keycode == TEST_DELAYED_ACTION_B);
    CHECK(tap_call.real == (uint8_t)(mods.real | fallback_hold_settle_real_mods));
    CHECK(tap_call.weak == (uint8_t)(mods.weak | fallback_hold_settle_weak_mods));
    CHECK(tap_call.oneshot == (uint8_t)(mods.oneshot | fallback_hold_settle_oneshot_mods));
    CHECK(tap_call.oneshot_locked == (uint8_t)(mods.oneshot_locked | fallback_hold_settle_oneshot_locked_mods));
    CHECK(fake_mods == 0x01);
    CHECK(fake_weak_mods == 0x02);
    CHECK(fake_oneshot_mods == 0x04);
    CHECK(fake_oneshot_locked_mods == 0x08);
}

int main(void) {
    test_delayed_action_restores_saved_mod_state_after_emit();
    test_delayed_action_settles_fallback_holds_inside_replay_window();
    puts("delayed_action host tests passed");
    return 0;
}
