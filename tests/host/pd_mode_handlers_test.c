#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/pointing/pd_mode_handlers.h"

#define TEST_MAX_CALLS 8

typedef struct {
    uint16_t keycode;
    uint8_t  real;
    uint8_t  weak;
    uint8_t  oneshot;
    uint8_t  oneshot_locked;
    bool     fallback_hold_active;
} synthetic_tap_call_t;

typedef struct {
    uint16_t keycode;
    uint8_t  real;
    uint8_t  weak;
    uint8_t  oneshot;
    uint8_t  oneshot_locked;
} literal_tap_call_t;

static uint8_t fake_mods;
static uint8_t fake_weak_mods;
static uint8_t fake_oneshot_mods;
static uint8_t fake_oneshot_locked_mods;

static uint8_t send_keyboard_report_count;

static synthetic_tap_call_t synthetic_tap_calls[TEST_MAX_CALLS];
static uint8_t              synthetic_tap_call_count;

static literal_tap_call_t literal_tap_calls[TEST_MAX_CALLS];
static uint8_t            literal_tap_call_count;

static uint8_t  fallback_hold_activation_count;
static bool     fallback_hold_active;

static uint8_t  keyboard_mod_register_count;
static uint8_t  keyboard_mod_unregister_count;
static uint16_t last_registered_keycode;
static uint16_t last_unregistered_keycode;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)            \
    do {                       \
        if (!(expr)) {         \
            test_fail(#expr, __FILE__, __LINE__); \
        }                      \
    } while (0)

static void test_clear_logs(void) {
    send_keyboard_report_count     = 0;
    synthetic_tap_call_count       = 0;
    literal_tap_call_count         = 0;
    fallback_hold_activation_count = 0;
    fallback_hold_active           = false;
    keyboard_mod_register_count    = 0;
    keyboard_mod_unregister_count  = 0;
    last_registered_keycode        = KC_NO;
    last_unregistered_keycode      = KC_NO;

    memset(synthetic_tap_calls, 0, sizeof(synthetic_tap_calls));
    memset(literal_tap_calls, 0, sizeof(literal_tap_calls));
}

static void test_reset_stubs(void) {
    fake_mods               = 0;
    fake_weak_mods          = 0;
    fake_oneshot_mods       = 0;
    fake_oneshot_locked_mods = 0;

    test_clear_logs();
    reset_arrow_mode();
    test_clear_logs();
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

bool key_runtime_effects_activate_pending_fallback_hold(void) {
    fallback_hold_activation_count++;
    fallback_hold_active = true;
    return true;
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    CHECK(synthetic_tap_call_count < TEST_MAX_CALLS);
    synthetic_tap_calls[synthetic_tap_call_count++] = (synthetic_tap_call_t){
        .keycode              = keycode,
        .real                 = fake_mods,
        .weak                 = fake_weak_mods,
        .oneshot              = fake_oneshot_mods,
        .oneshot_locked       = fake_oneshot_locked_mods,
        .fallback_hold_active = fallback_hold_active,
    };
}

void tap_code16(uint16_t keycode) {
    CHECK(literal_tap_call_count < TEST_MAX_CALLS);
    literal_tap_calls[literal_tap_call_count++] = (literal_tap_call_t){
        .keycode        = keycode,
        .real           = fake_mods,
        .weak           = fake_weak_mods,
        .oneshot        = fake_oneshot_mods,
        .oneshot_locked = fake_oneshot_locked_mods,
    };
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    keyboard_mod_register_count++;
    last_registered_keycode = keycode;
    add_mods(MOD_BIT(keycode));
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    keyboard_mod_unregister_count++;
    last_unregistered_keycode = keycode;
    del_mods(MOD_BIT(keycode));
}

static keyrecord_t test_record(bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .key =
                    {
                        .row = 1,
                        .col = 2,
                    },
                .pressed = pressed,
            },
    };
}

static void test_horizontal_arrow_tap_preserves_mod_state(void) {
    test_reset_stubs();

    fake_mods                = MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    handle_arrow_mode((report_mouse_t){
        .x = 40,
        .y = 0,
    });

    CHECK(synthetic_tap_call_count == 1);
    CHECK(synthetic_tap_calls[0].keycode == KC_RIGHT);
    CHECK(synthetic_tap_calls[0].real == (MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT)));
    CHECK(synthetic_tap_calls[0].weak == MOD_BIT(KC_RIGHT_ALT));
    CHECK(synthetic_tap_calls[0].oneshot == MOD_BIT(KC_LEFT_GUI));
    CHECK(synthetic_tap_calls[0].oneshot_locked == MOD_BIT(KC_RIGHT_GUI));
    CHECK(synthetic_tap_calls[0].fallback_hold_active);
    CHECK(fallback_hold_activation_count == 1);
    CHECK(send_keyboard_report_count == 0);
}

static void test_vertical_arrow_tap_masks_alt_and_restores_mod_state(void) {
    test_reset_stubs();

    fake_mods                = MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_ALT);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_ALT) | MOD_BIT(KC_RIGHT_SHIFT);

    handle_arrow_mode((report_mouse_t){
        .x = 0,
        .y = 50,
    });

    CHECK(synthetic_tap_call_count == 1);
    CHECK(synthetic_tap_calls[0].keycode == KC_DOWN);
    CHECK(synthetic_tap_calls[0].real == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(synthetic_tap_calls[0].weak == 0);
    CHECK(synthetic_tap_calls[0].oneshot == 0);
    CHECK(synthetic_tap_calls[0].oneshot_locked == MOD_BIT(KC_RIGHT_SHIFT));
    CHECK(synthetic_tap_calls[0].fallback_hold_active);

    CHECK(fake_mods == (MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT)));
    CHECK(fake_weak_mods == MOD_BIT(KC_RIGHT_ALT));
    CHECK(fake_oneshot_mods == MOD_BIT(KC_LEFT_ALT));
    CHECK(fake_oneshot_locked_mods == (MOD_BIT(KC_RIGHT_ALT) | MOD_BIT(KC_RIGHT_SHIFT)));
    CHECK(fallback_hold_activation_count == 1);
    CHECK(send_keyboard_report_count == 3);
}

static void test_arrow_mode_selection_button_holds_and_releases_shift(void) {
    keyrecord_t press   = test_record(true);
    keyrecord_t release = test_record(false);

    test_reset_stubs();

    CHECK(handle_arrow_mode_key(MS_BTN1, &press));
    CHECK(keyboard_mod_register_count == 1);
    CHECK(last_registered_keycode == KC_RIGHT_SHIFT);
    CHECK((fake_mods & MOD_BIT(KC_RIGHT_SHIFT)) != 0);

    CHECK(handle_arrow_mode_key(MS_BTN1, &release));
    CHECK(keyboard_mod_unregister_count == 1);
    CHECK(last_unregistered_keycode == KC_RIGHT_SHIFT);
    CHECK((fake_mods & MOD_BIT(KC_RIGHT_SHIFT)) == 0);
}

static void test_arrow_mode_copy_shortcut_suspends_ambient_mods(void) {
    keyrecord_t press = test_record(true);

    test_reset_stubs();

    fake_mods                = MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    CHECK(handle_arrow_mode_key(MS_BTN2, &press));
    CHECK(literal_tap_call_count == 1);
    CHECK(literal_tap_calls[0].keycode == G(KC_C));
    CHECK(literal_tap_calls[0].real == 0);
    CHECK(literal_tap_calls[0].weak == 0);
    CHECK(literal_tap_calls[0].oneshot == 0);
    CHECK(literal_tap_calls[0].oneshot_locked == 0);

    CHECK(fake_mods == (MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT)));
    CHECK(fake_weak_mods == MOD_BIT(KC_RIGHT_ALT));
    CHECK(fake_oneshot_mods == MOD_BIT(KC_LEFT_GUI));
    CHECK(fake_oneshot_locked_mods == MOD_BIT(KC_RIGHT_GUI));
    CHECK(send_keyboard_report_count == 2);
}

int main(void) {
    test_horizontal_arrow_tap_preserves_mod_state();
    test_vertical_arrow_tap_masks_alt_and_restores_mod_state();
    test_arrow_mode_selection_button_holds_and_releases_shift();
    test_arrow_mode_copy_shortcut_suspends_ambient_mods();

    puts("pd_mode_handlers host tests passed");
    return 0;
}
