#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"

typedef struct {
    uint16_t keycode;
    bool     fallback_hold_active;
    uint8_t  real;
    uint8_t  weak;
    uint8_t  oneshot;
    uint8_t  oneshot_locked;
} tap_call_t;

static uint8_t fake_mods;
static uint8_t fake_weak_mods;
static uint8_t fake_oneshot_mods;
static uint8_t fake_oneshot_locked_mods;
static uint8_t send_keyboard_report_count;

static uint8_t fallback_hold_activation_count;
static bool    fallback_hold_active;

static bool layer_locked_state[LAYER_COUNT];

static tap_call_t action_tap_call;
static tap_call_t synthetic_qmk_tap_call;
static tap_call_t literal_tap_call;

static uint8_t action_tap_call_count;
static uint8_t synthetic_qmk_tap_call_count;
static uint8_t literal_tap_call_count;

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
    fake_mods                    = 0;
    fake_weak_mods               = 0;
    fake_oneshot_mods            = 0;
    fake_oneshot_locked_mods     = 0;
    send_keyboard_report_count   = 0;
    fallback_hold_activation_count = 0;
    fallback_hold_active         = false;
    action_tap_call              = (tap_call_t){0};
    synthetic_qmk_tap_call       = (tap_call_t){0};
    literal_tap_call             = (tap_call_t){0};
    action_tap_call_count        = 0;
    synthetic_qmk_tap_call_count = 0;
    literal_tap_call_count       = 0;

    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        layer_locked_state[layer] = false;
    }
}

static tap_call_t test_current_tap_call(uint16_t keycode) {
    return (tap_call_t){
        .keycode              = keycode,
        .fallback_hold_active = fallback_hold_active,
        .real                 = fake_mods,
        .weak                 = fake_weak_mods,
        .oneshot              = fake_oneshot_mods,
        .oneshot_locked       = fake_oneshot_locked_mods,
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

void send_keyboard_report(void) {
    send_keyboard_report_count++;
}

bool key_runtime_activate_pending_fallback_hold(void) {
    fallback_hold_activation_count++;
    fallback_hold_active = true;
    return true;
}

bool layer_ownership_is_locked(uint8_t layer) {
    return layer < LAYER_COUNT && layer_locked_state[layer];
}

void noah_action_tap(uint16_t action) {
    action_tap_call = test_current_tap_call(action);
    action_tap_call_count++;
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    synthetic_qmk_tap_call = test_current_tap_call(keycode);
    synthetic_qmk_tap_call_count++;
}

void tap_code16(uint16_t keycode) {
    literal_tap_call = test_current_tap_call(keycode);
    literal_tap_call_count++;
}

static void test_action_dispatch_keeps_runtime_default_policy(void) {
    test_reset_stubs();

    action_dispatch(KC_C);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(action_tap_call_count == 1);
    CHECK(action_tap_call.keycode == KC_C);
    CHECK(action_tap_call.fallback_hold_active);
}

static void test_explicit_action_emit_can_skip_fallback_hold_settlement(void) {
    test_reset_stubs();

    noah_emit_action_tap(KC_RIGHT, NOAH_EMIT_POLICY_NONE);

    CHECK(fallback_hold_activation_count == 0);
    CHECK(action_tap_call_count == 1);
    CHECK(action_tap_call.keycode == KC_RIGHT);
    CHECK(!action_tap_call.fallback_hold_active);
}

static void test_synthetic_qmk_emit_settles_fallback_hold_without_touching_mod_state(void) {
    test_reset_stubs();
    fake_mods                = MOD_BIT(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    noah_emit_synthetic_qmk_tap(KC_RIGHT, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(synthetic_qmk_tap_call_count == 1);
    CHECK(synthetic_qmk_tap_call.keycode == KC_RIGHT);
    CHECK(synthetic_qmk_tap_call.fallback_hold_active);
    CHECK(synthetic_qmk_tap_call.real == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(synthetic_qmk_tap_call.weak == MOD_BIT(KC_RIGHT_ALT));
    CHECK(synthetic_qmk_tap_call.oneshot == MOD_BIT(KC_LEFT_GUI));
    CHECK(synthetic_qmk_tap_call.oneshot_locked == MOD_BIT(KC_RIGHT_GUI));
    CHECK(send_keyboard_report_count == 0);
}

static void test_literal_emit_can_suspend_and_restore_mods(void) {
    test_reset_stubs();
    fake_mods                = MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    noah_emit_literal_tap(G(KC_C), NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(literal_tap_call_count == 1);
    CHECK(literal_tap_call.keycode == G(KC_C));
    CHECK(literal_tap_call.fallback_hold_active);
    CHECK(literal_tap_call.real == 0);
    CHECK(literal_tap_call.weak == 0);
    CHECK(literal_tap_call.oneshot == 0);
    CHECK(literal_tap_call.oneshot_locked == 0);
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT)));
    CHECK(fake_weak_mods == MOD_BIT(KC_RIGHT_ALT));
    CHECK(fake_oneshot_mods == MOD_BIT(KC_LEFT_GUI));
    CHECK(fake_oneshot_locked_mods == MOD_BIT(KC_RIGHT_GUI));
    CHECK(send_keyboard_report_count == 2);
}

int main(void) {
    test_action_dispatch_keeps_runtime_default_policy();
    test_explicit_action_emit_can_skip_fallback_hold_settlement();
    test_synthetic_qmk_emit_settles_fallback_hold_without_touching_mod_state();
    test_literal_emit_can_suspend_and_restore_mods();

    puts("action_dispatch host tests passed");
    return 0;
}
