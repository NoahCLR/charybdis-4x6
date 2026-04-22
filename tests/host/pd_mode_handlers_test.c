#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_runtime_reset_fixture.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/pointing/modes/pd_mode_handlers.h"
#include "users/noah/lib/state/runtime/keyboard_mod_state.h"

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

static host_runtime_fixture_t runtime_fixture = HOST_RUNTIME_FIXTURE_INIT;

#define fake_mods runtime_fixture.mods
#define fake_weak_mods runtime_fixture.weak_mods
#define fake_oneshot_mods runtime_fixture.oneshot_mods
#define fake_oneshot_locked_mods runtime_fixture.oneshot_locked_mods
#define fake_time32 runtime_fixture.time32

static synthetic_tap_call_t synthetic_tap_calls[TEST_MAX_CALLS];
static uint8_t              synthetic_tap_call_count;

static literal_tap_call_t literal_tap_calls[TEST_MAX_CALLS];
static uint8_t            literal_tap_call_count;

static uint8_t fallback_hold_activation_count;
static bool    fallback_hold_active;

static uint8_t  keyboard_mod_register_count;
static uint8_t  keyboard_mod_unregister_count;
static uint16_t last_registered_keycode;
static uint16_t last_unregistered_keycode;

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

static void test_clear_logs(void) {
    runtime_fixture.send_keyboard_report_count = 0;
    synthetic_tap_call_count                   = 0;
    literal_tap_call_count                     = 0;
    fallback_hold_activation_count             = 0;
    fallback_hold_active                       = false;
    fake_time32                                = 1000u;
    keyboard_mod_register_count                = 0;
    keyboard_mod_unregister_count              = 0;
    last_registered_keycode                    = KC_NO;
    last_unregistered_keycode                  = KC_NO;

    memset(synthetic_tap_calls, 0, sizeof(synthetic_tap_calls));
    memset(literal_tap_calls, 0, sizeof(literal_tap_calls));
}

static void test_reset_stubs(void) {
    host_runtime_fixture_reset(&runtime_fixture);
    test_clear_logs();
    reset_dragscroll_mode();
    reset_arrow_mode();
    test_clear_logs();
}

HOST_RUNTIME_FIXTURE_DEFINE_BASIC_QMK_STUBS(runtime_fixture)

bool noah_key_runtime_settle_pending_fallback_hold(void) {
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

void noah_emit_synthetic_qmk_tap(uint16_t keycode, noah_emit_policy_t policy) {
    if (policy.settle_pending_fallback_holds) {
        noah_key_runtime_settle_pending_fallback_hold();
    }

    if (policy.preserve_keyboard_mod_state) {
        test_fail("pd_mode synthetic tap should not preserve mod state internally", __FILE__, __LINE__);
    }

    noah_dispatch_synthetic_qmk_tap(keycode);
}

void noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(uint16_t keycode, uint8_t masked_mods, bool settle_pending_fallback_holds) {
    keyboard_mod_state_t saved = {
        .real           = fake_mods,
        .weak           = fake_weak_mods,
        .oneshot        = fake_oneshot_mods,
        .oneshot_locked = fake_oneshot_locked_mods,
    };
    keyboard_mod_state_t filtered = saved;

    if (settle_pending_fallback_holds) {
        noah_key_runtime_settle_pending_fallback_hold();
        saved = (keyboard_mod_state_t){
            .real           = fake_mods,
            .weak           = fake_weak_mods,
            .oneshot        = fake_oneshot_mods,
            .oneshot_locked = fake_oneshot_locked_mods,
        };
        filtered = saved;
    }

    filtered.real &= (uint8_t)~masked_mods;
    filtered.weak &= (uint8_t)~masked_mods;
    filtered.oneshot &= (uint8_t)~masked_mods;
    filtered.oneshot_locked &= (uint8_t)~masked_mods;

    keyboard_mod_state_apply(filtered);
    noah_dispatch_synthetic_qmk_tap(keycode);
    keyboard_mod_state_apply(saved);
}

void noah_emit_literal_tap(uint16_t keycode, noah_emit_policy_t policy) {
    if (policy.settle_pending_fallback_holds) {
        noah_key_runtime_settle_pending_fallback_hold();
    }

    if (policy.preserve_keyboard_mod_state) {
        keyboard_mod_state_t saved = keyboard_mod_state_suspend();
        tap_code16(keycode);
        keyboard_mod_state_apply(saved);
        return;
    }

    tap_code16(keycode);
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
                .type    = KEY_EVENT,
                .pressed = pressed,
            },
    };
}

static void test_dragscroll_horizontal_lock_filters_vertical_jitter(void) {
    test_reset_stubs();

    report_mouse_t report;

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 18,
        .y = 2,
    });
    CHECK(report.x == 0);
    CHECK(report.y == 0);
    CHECK(report.h == 3);
    CHECK(report.v == 0);

    fake_time32 = 1040u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 6,
        .y = 4,
    });
    CHECK(report.h == 1);
    CHECK(report.v == 0);
}

static void test_dragscroll_vertical_lock_filters_horizontal_jitter(void) {
    test_reset_stubs();

    report_mouse_t report;

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 2,
        .y = -24,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 3);

    fake_time32 = 1040u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 5,
        .y = -8,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 1);
}

static void test_dragscroll_near_diagonal_motion_waits_for_dominant_axis(void) {
    test_reset_stubs();

    report_mouse_t report;

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 8,
        .y = 7,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 0);

    fake_time32 = 1040u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 6,
        .y = 0,
    });
    CHECK(report.h == 2);
    CHECK(report.v == 0);
}

static void test_dragscroll_opposite_axis_does_not_steal_active_lock(void) {
    test_reset_stubs();

    report_mouse_t report;

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 22,
        .y = 1,
    });
    CHECK(report.h == 3);
    CHECK(report.v == 0);

    fake_time32 = 1040u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 11,
        .y = 12,
    });
    CHECK(report.h == 2);
    CHECK(report.v == 0);
}

static void test_dragscroll_ambiguous_wobble_keeps_existing_lock(void) {
    test_reset_stubs();

    report_mouse_t report;

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 18,
        .y = 1,
    });
    CHECK(report.h == 3);
    CHECK(report.v == 0);

    fake_time32 = 1040u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 7,
        .y = 7,
    });
    CHECK(report.h == 1);
    CHECK(report.v == 0);
}

static void test_dragscroll_lock_releases_after_pause_and_switches_axes_cleanly(void) {
    test_reset_stubs();

    report_mouse_t report;

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 18,
        .y = 1,
    });
    CHECK(report.h == 3);
    CHECK(report.v == 0);

    fake_time32 = 1080u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = 0,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 0);

    fake_time32 = 1100u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -24,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 3);
}

static void test_dragscroll_uses_different_horizontal_and_vertical_divisors(void) {
    test_reset_stubs();

    report_mouse_t report;

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 12,
        .y = 0,
    });
    CHECK(report.h == 2);
    CHECK(report.v == 0);

    reset_dragscroll_mode();

    fake_time32 = 1040u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -12,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 1);
}

static void test_dragscroll_cross_axis_decay_prevents_residual_leakage(void) {
    test_reset_stubs();

    report_mouse_t report;

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 18,
        .y = 8,
    });
    CHECK(report.h == 3);
    CHECK(report.v == 0);

    fake_time32 = 1040u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 6,
        .y = 0,
    });
    CHECK(report.h == 1);
    CHECK(report.v == 0);

    fake_time32 = 1060u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = 0,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 0);
}

static void test_dragscroll_reset_clears_buffers_and_lock_state(void) {
    test_reset_stubs();

    report_mouse_t report;

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 18,
        .y = 1,
    });
    CHECK(report.h == 3);
    CHECK(report.v == 0);

    reset_dragscroll_mode();

    fake_time32 = 1040u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -16,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 2);
}

static void test_volume_mode_emits_discrete_steps_and_resets_on_direction_change(void) {
    report_mouse_t report;

    test_reset_stubs();

    report = handle_volume_mode((report_mouse_t){
        .y = 60,
    });
    CHECK(report.x == 0);
    CHECK(report.y == 0);
    CHECK(synthetic_tap_call_count == 1);
    CHECK(synthetic_tap_calls[0].keycode == KC_AUDIO_VOL_DOWN);
    CHECK(synthetic_tap_calls[0].fallback_hold_active);

    test_clear_logs();

    report = handle_volume_mode((report_mouse_t){
        .y = -60,
    });
    CHECK(report.x == 0);
    CHECK(report.y == 0);
    CHECK(synthetic_tap_call_count == 1);
    CHECK(synthetic_tap_calls[0].keycode == KC_AUDIO_VOL_UP);
    CHECK(synthetic_tap_calls[0].fallback_hold_active);

    test_clear_logs();

    report = handle_volume_mode((report_mouse_t){
        .y = 120,
    });
    CHECK(report.x == 0);
    CHECK(report.y == 0);
    CHECK(synthetic_tap_call_count == 2);
    CHECK(synthetic_tap_calls[0].keycode == KC_AUDIO_VOL_DOWN);
    CHECK(synthetic_tap_calls[1].keycode == KC_AUDIO_VOL_DOWN);
}

static void test_brightness_mode_emits_discrete_steps_and_resets_on_direction_change(void) {
    report_mouse_t report;

    test_reset_stubs();

    report = handle_brightness_mode((report_mouse_t){
        .y = 60,
    });
    CHECK(report.x == 0);
    CHECK(report.y == 0);
    CHECK(synthetic_tap_call_count == 1);
    CHECK(synthetic_tap_calls[0].keycode == KC_BRID);
    CHECK(synthetic_tap_calls[0].fallback_hold_active);

    test_clear_logs();

    report = handle_brightness_mode((report_mouse_t){
        .y = -60,
    });
    CHECK(report.x == 0);
    CHECK(report.y == 0);
    CHECK(synthetic_tap_call_count == 1);
    CHECK(synthetic_tap_calls[0].keycode == KC_BRIU);
    CHECK(synthetic_tap_calls[0].fallback_hold_active);
}

static void test_zoom_mode_emits_discrete_steps_and_resets_on_direction_change(void) {
    report_mouse_t report;

    test_reset_stubs();

    report = handle_zoom_mode((report_mouse_t){
        .y = 80,
    });
    CHECK(report.x == 0);
    CHECK(report.y == 0);
    CHECK(synthetic_tap_call_count == 1);
    CHECK(synthetic_tap_calls[0].keycode == G(KC_MINS));
    CHECK(synthetic_tap_calls[0].fallback_hold_active);

    test_clear_logs();

    report = handle_zoom_mode((report_mouse_t){
        .y = -80,
    });
    CHECK(report.x == 0);
    CHECK(report.y == 0);
    report = handle_zoom_mode((report_mouse_t){
        .y = -80,
    });
    CHECK(report.x == 0);
    CHECK(report.y == 0);
    CHECK(synthetic_tap_call_count == 2);
    CHECK(synthetic_tap_calls[0].keycode == G(KC_EQL));
    CHECK(synthetic_tap_calls[1].keycode == G(KC_EQL));
    CHECK(synthetic_tap_calls[0].fallback_hold_active);
    CHECK(synthetic_tap_calls[1].fallback_hold_active);
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
    CHECK(runtime_fixture.send_keyboard_report_count == 0);
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
    CHECK(runtime_fixture.send_keyboard_report_count == 2);
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
    CHECK(fallback_hold_activation_count == 1);
    CHECK(fallback_hold_active);
    CHECK(runtime_fixture.send_keyboard_report_count == 2);
}

int main(void) {
    test_dragscroll_horizontal_lock_filters_vertical_jitter();
    test_dragscroll_vertical_lock_filters_horizontal_jitter();
    test_dragscroll_near_diagonal_motion_waits_for_dominant_axis();
    test_dragscroll_opposite_axis_does_not_steal_active_lock();
    test_dragscroll_ambiguous_wobble_keeps_existing_lock();
    test_dragscroll_lock_releases_after_pause_and_switches_axes_cleanly();
    test_dragscroll_uses_different_horizontal_and_vertical_divisors();
    test_dragscroll_cross_axis_decay_prevents_residual_leakage();
    test_dragscroll_reset_clears_buffers_and_lock_state();
    test_volume_mode_emits_discrete_steps_and_resets_on_direction_change();
    test_brightness_mode_emits_discrete_steps_and_resets_on_direction_change();
    test_zoom_mode_emits_discrete_steps_and_resets_on_direction_change();
    test_horizontal_arrow_tap_preserves_mod_state();
    test_vertical_arrow_tap_masks_alt_and_restores_mod_state();
    test_arrow_mode_selection_button_holds_and_releases_shift();
    test_arrow_mode_copy_shortcut_suspends_ambient_mods();

    puts("pd_mode_handlers host tests passed");
    return 0;
}
