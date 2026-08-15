#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_runtime_reset_fixture.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/pointing/modes/pd_mode_handler_common.h"
#include "users/noah/lib/pointing/modes/pd_mode_handlers.h"
#include "users/noah/lib/state/modifiers/keyboard_mod_state.h"

#define TEST_MAX_CALLS 8

// A 16-bit mouse report can carry 32767 counts, so unsaturated int32_t residual
// accumulation wraps after roughly 65537 maximum reports. The extreme-input
// tests push past that boundary; a saturating accumulator has to keep the
// gesture pointing the same way instead of flipping sign.
#define TEST_DRAGSCROLL_OVERFLOW_REPORTS 70000u

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
    runtime_fixture.timer_read32_count         = 0u;
    runtime_fixture.timer_elapsed32_count      = 0u;
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
    reset_volume_mode();
    reset_brightness_mode();
    reset_zoom_mode();
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

bool owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease) {
    CHECK(lease != NULL);
    keyboard_mod_register_count++;
    last_registered_keycode = keycode;
    add_mods(MOD_BIT(keycode));
    *lease = (owned_keycode_lease_t){.active = true, .has_basic = true, .basic = (uint8_t)keycode};
    return true;
}

bool owned_keycode_release(owned_keycode_lease_t *lease) {
    CHECK(lease != NULL);
    CHECK(lease->active);
    keyboard_mod_unregister_count++;
    last_unregistered_keycode = lease->basic;
    del_mods(MOD_BIT(lease->basic));
    *lease = (owned_keycode_lease_t){0};
    return true;
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

static void test_assert_synthetic_calls(uint8_t expected_count, uint16_t expected_keycode) {
    CHECK(synthetic_tap_call_count == expected_count);
    CHECK(fallback_hold_activation_count == expected_count);

    for (uint8_t i = 0; i < synthetic_tap_call_count; i++) {
        CHECK(synthetic_tap_calls[i].keycode == expected_keycode);
        CHECK(synthetic_tap_calls[i].fallback_hold_active);
    }
}

static void test_dragscroll_prime_horizontal_lock_with_residual(uint32_t start_time) {
    report_mouse_t report;

    fake_time32 = start_time;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 18,
        .y = 0,
    });
    CHECK(report.h == 3);
    CHECK(report.v == 0);

    fake_time32 = start_time + 1u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 6,
        .y = 0,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 0);
}

static void test_dragscroll_drive_reports(int16_t dx, int16_t dy, uint32_t count) {
    for (uint32_t index = 0; index < count; index++) {
        (void)handle_dragscroll_mode((report_mouse_t){
            .x = dx,
            .y = dy,
        });
    }
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

static void test_dragscroll_lock_timeout_boundary_uses_prior_motion_age(void) {
    report_mouse_t report;

    test_reset_stubs();
    test_dragscroll_prime_horizontal_lock_with_residual(1020u);

    fake_time32 = 1021u + NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -4,
    });
    CHECK(report.h == 1);
    CHECK(report.v == 0);

    test_reset_stubs();
    test_dragscroll_prime_horizontal_lock_with_residual(1020u);

    fake_time32 = 1021u + NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS + 1u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -4,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 0);
}

static void test_dragscroll_buffer_timeout_boundary_preserves_then_expires_residual(void) {
    report_mouse_t report;

    test_reset_stubs();
    test_dragscroll_prime_horizontal_lock_with_residual(1020u);

    fake_time32 = 1021u + NOAH_DRAGSCROLL_BUFFER_EXPIRE_MS;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -4,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 0);

    fake_time32 += NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report = handle_dragscroll_mode((report_mouse_t){
        .x = 6,
        .y = 0,
    });
    CHECK(report.h == 2);
    CHECK(report.v == 0);

    test_reset_stubs();
    test_dragscroll_prime_horizontal_lock_with_residual(1020u);

    fake_time32 = 1021u + NOAH_DRAGSCROLL_BUFFER_EXPIRE_MS + 1u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -4,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 0);

    fake_time32 += NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -4,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 1);
}

static void test_dragscroll_no_motion_expires_stale_state(void) {
    report_mouse_t report;

    test_reset_stubs();
    test_dragscroll_prime_horizontal_lock_with_residual(1020u);

    fake_time32 = 1021u + NOAH_DRAGSCROLL_BUFFER_EXPIRE_MS + 1u;
    report      = handle_dragscroll_mode((report_mouse_t){0});
    CHECK(report.h == 0);
    CHECK(report.v == 0);

    fake_time32 += NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -8,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 1);
}

static void test_dragscroll_first_post_expiry_report_can_emit_new_axis(void) {
    report_mouse_t report;

    test_reset_stubs();
    test_dragscroll_prime_horizontal_lock_with_residual(1020u);

    fake_time32 = 1021u + NOAH_DRAGSCROLL_BUFFER_EXPIRE_MS + 1u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -16,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 2);
}

static void test_dragscroll_rate_limit_boundary_is_inclusive(void) {
    report_mouse_t report;

    test_reset_stubs();

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 18,
        .y = 0,
    });
    CHECK(report.h == 3);

    fake_time32 = 1020u + NOAH_DRAGSCROLL_RATE_LIMIT_MS - 1u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 6,
        .y = 0,
    });
    CHECK(report.h == 0);

    fake_time32 = 1020u + NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report      = handle_dragscroll_mode((report_mouse_t){0});
    CHECK(report.h == 1);
    CHECK(report.v == 0);
}

static void test_dragscroll_prior_motion_age_is_wrap_safe(void) {
    report_mouse_t report;
    uint32_t       start_time = UINT32_MAX - 32u;

    test_reset_stubs();
    test_dragscroll_prime_horizontal_lock_with_residual(start_time);

    fake_time32 = start_time + 1u + NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS + 1u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = 0,
        .y = -4,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 0);
}

static void test_dragscroll_uses_one_timer_sample_per_invocation(void) {
    test_reset_stubs();

    (void)handle_dragscroll_mode((report_mouse_t){
        .x = 18,
        .y = 0,
    });

    CHECK(runtime_fixture.timer_read32_count == 1u);
    CHECK(runtime_fixture.timer_elapsed32_count == 0u);
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

static void test_dragscroll_extreme_input_saturates_instead_of_wrapping(void) {
    report_mouse_t report;

    test_reset_stubs();

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = INT16_MAX,
    });
    CHECK(report.h > 0);
    CHECK(report.v == 0);

    // The rate limit blocks the drain, so these reports only accumulate.
    test_dragscroll_drive_reports(INT16_MAX, 0, TEST_DRAGSCROLL_OVERFLOW_REPORTS);

    fake_time32 = 1020u + NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report      = handle_dragscroll_mode((report_mouse_t){0});
    CHECK(report.h > 0);
    CHECK(report.v == 0);

    // A buffer already sitting at the cap still accepts full reports and still
    // scrolls the same way.
    test_dragscroll_drive_reports(INT16_MAX, 0, TEST_DRAGSCROLL_OVERFLOW_REPORTS);

    fake_time32 = 1020u + 2u * NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report      = handle_dragscroll_mode((report_mouse_t){0});
    CHECK(report.h > 0);
    CHECK(report.v == 0);
}

static void test_dragscroll_reverse_axis_extremes_saturate_in_both_directions(void) {
    report_mouse_t report;

    test_reset_stubs();

    // NOAH_DRAGSCROLL_REVERSE_Y subtracts the report, so positive vertical input
    // drives the residual buffer negative.
    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .y = INT16_MAX,
    });
    CHECK(report.h == 0);
    CHECK(report.v < 0);

    test_dragscroll_drive_reports(0, INT16_MAX, TEST_DRAGSCROLL_OVERFLOW_REPORTS);

    fake_time32 = 1020u + NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report      = handle_dragscroll_mode((report_mouse_t){0});
    CHECK(report.h == 0);
    CHECK(report.v < 0);

    // Reversing walks the saturated buffer back through zero to the other cap
    // instead of sticking or wrapping.
    test_dragscroll_drive_reports(0, INT16_MIN, TEST_DRAGSCROLL_OVERFLOW_REPORTS);

    fake_time32 = 1020u + 2u * NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report      = handle_dragscroll_mode((report_mouse_t){0});
    CHECK(report.h == 0);
    CHECK(report.v > 0);
}

static void test_dragscroll_saturated_buffer_expires_after_pause(void) {
    report_mouse_t report;

    test_reset_stubs();

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .x = INT16_MAX,
    });
    CHECK(report.h > 0);

    test_dragscroll_drive_reports(INT16_MAX, 0, TEST_DRAGSCROLL_OVERFLOW_REPORTS);

    fake_time32 = 1020u + NOAH_DRAGSCROLL_BUFFER_EXPIRE_MS + 1u;
    report      = handle_dragscroll_mode((report_mouse_t){0});
    CHECK(report.h == 0);
    CHECK(report.v == 0);

    fake_time32 += NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report = handle_dragscroll_mode((report_mouse_t){
        .y = -24,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 3);
}

static void test_pinch_reuse_shares_saturating_dragscroll_accumulator(void) {
    report_mouse_t report;

    // PINCH reuses handle_dragscroll_mode() and reset_dragscroll_mode() through
    // the pd mode manifest, so the pinch gesture shares this residual cap. The
    // registry mapping itself is asserted in pd_mode_test.c.
    test_reset_stubs();

    fake_time32 = 1020u;
    report      = handle_dragscroll_mode((report_mouse_t){
        .y = INT16_MIN,
    });
    CHECK(report.h == 0);
    CHECK(report.v > 0);

    test_dragscroll_drive_reports(0, INT16_MIN, TEST_DRAGSCROLL_OVERFLOW_REPORTS);

    fake_time32 = 1020u + NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report      = handle_dragscroll_mode((report_mouse_t){0});
    CHECK(report.h == 0);
    CHECK(report.v > 0);

    reset_dragscroll_mode();

    fake_time32 += NOAH_DRAGSCROLL_RATE_LIMIT_MS;
    report = handle_dragscroll_mode((report_mouse_t){
        .y = -24,
    });
    CHECK(report.h == 0);
    CHECK(report.v == 3);
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

static void test_maximum_volume_report_respects_per_tick_budget(void) {
    pd_mode_axis_debug_snapshot_t snapshot;

    test_reset_stubs();

    (void)handle_volume_mode((report_mouse_t){
        .y = INT16_MAX,
    });

    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_AUDIO_VOL_DOWN);
    pd_mode_volume_debug_snapshot(&snapshot);
    CHECK(snapshot.accumulated_motion == (int32_t)((NOAH_PD_MODE_MAX_BACKLOG_TAPS - NOAH_PD_MODE_MAX_TAPS_PER_TICK) * 60u + (INT16_MAX % 60)));
    CHECK(snapshot.pending_tap_count == NOAH_PD_MODE_MAX_BACKLOG_TAPS - NOAH_PD_MODE_MAX_TAPS_PER_TICK);
    CHECK(snapshot.maximum_backlog_taps == NOAH_PD_MODE_MAX_BACKLOG_TAPS);
    CHECK(snapshot.maximum_taps_emitted == NOAH_PD_MODE_MAX_TAPS_PER_TICK);
    CHECK(snapshot.saturated_report_count == 1u);
    CHECK(snapshot.discarded_tap_count == (uint32_t)(INT16_MAX / 60) - NOAH_PD_MODE_MAX_BACKLOG_TAPS);
    CHECK(snapshot.direction == 1);

    test_clear_logs();

    (void)handle_volume_mode((report_mouse_t){
        .y = INT16_MIN,
    });

    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_AUDIO_VOL_UP);
    pd_mode_volume_debug_snapshot(&snapshot);
    CHECK(snapshot.accumulated_motion == -(int32_t)((NOAH_PD_MODE_MAX_BACKLOG_TAPS - NOAH_PD_MODE_MAX_TAPS_PER_TICK) * 60u + ((uint32_t)INT16_MAX + 1u) % 60u));
    CHECK(snapshot.pending_tap_count == NOAH_PD_MODE_MAX_BACKLOG_TAPS - NOAH_PD_MODE_MAX_TAPS_PER_TICK);
    CHECK(snapshot.saturated_report_count == 2u);
    CHECK(snapshot.discarded_tap_count == 2u * (((uint32_t)INT16_MAX + 1u) / 60u - NOAH_PD_MODE_MAX_BACKLOG_TAPS));
    CHECK(snapshot.direction == -1);
}

static void test_maximum_reports_are_bounded_in_every_discrete_mode(void) {
    test_reset_stubs();
    (void)handle_brightness_mode((report_mouse_t){.y = INT16_MAX});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_BRID);

    test_reset_stubs();
    (void)handle_brightness_mode((report_mouse_t){.y = INT16_MIN});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_BRIU);

    test_reset_stubs();
    (void)handle_zoom_mode((report_mouse_t){.y = INT16_MAX});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, G(KC_MINS));

    test_reset_stubs();
    (void)handle_zoom_mode((report_mouse_t){.y = INT16_MIN});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, G(KC_EQL));

    test_reset_stubs();
    (void)handle_arrow_mode((report_mouse_t){.x = INT16_MAX});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_RIGHT);

    test_reset_stubs();
    (void)handle_arrow_mode((report_mouse_t){.x = INT16_MIN});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_LEFT);

    test_reset_stubs();
    (void)handle_arrow_mode((report_mouse_t){.y = INT16_MAX});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_DOWN);

    test_reset_stubs();
    (void)handle_arrow_mode((report_mouse_t){.y = INT16_MIN});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_UP);
}

static void test_zero_motion_drains_bounded_backlog_and_preserves_residual(void) {
    pd_mode_axis_debug_snapshot_t snapshot;

    test_reset_stubs();
    (void)handle_volume_mode((report_mouse_t){.y = INT16_MAX});

    for (uint8_t tick = 0; tick < (NOAH_PD_MODE_MAX_BACKLOG_TAPS / NOAH_PD_MODE_MAX_TAPS_PER_TICK) - 1u; tick++) {
        test_clear_logs();
        (void)handle_volume_mode((report_mouse_t){0});
        test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_AUDIO_VOL_DOWN);
    }

    test_clear_logs();
    (void)handle_volume_mode((report_mouse_t){0});
    CHECK(synthetic_tap_call_count == 0u);

    pd_mode_volume_debug_snapshot(&snapshot);
    CHECK(snapshot.accumulated_motion == INT16_MAX % 60);
    CHECK(snapshot.pending_tap_count == 0u);
    CHECK(snapshot.saturated_report_count == 1u);
    CHECK(snapshot.maximum_backlog_taps == NOAH_PD_MODE_MAX_BACKLOG_TAPS);
    CHECK(snapshot.maximum_taps_emitted == NOAH_PD_MODE_MAX_TAPS_PER_TICK);
}

static void test_sustained_maximum_input_stays_bounded_and_observable(void) {
    pd_mode_axis_debug_snapshot_t snapshot;

    test_reset_stubs();

    for (uint8_t report_index = 0; report_index < 64u; report_index++) {
        test_clear_logs();
        (void)handle_volume_mode((report_mouse_t){.y = INT16_MAX});
        test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_AUDIO_VOL_DOWN);

        pd_mode_volume_debug_snapshot(&snapshot);
        CHECK(snapshot.pending_tap_count <= NOAH_PD_MODE_MAX_BACKLOG_TAPS);
        CHECK(snapshot.maximum_backlog_taps == NOAH_PD_MODE_MAX_BACKLOG_TAPS);
        CHECK(snapshot.maximum_taps_emitted == NOAH_PD_MODE_MAX_TAPS_PER_TICK);
    }

    CHECK(snapshot.saturated_report_count == 64u);
    CHECK(snapshot.discarded_tap_count > 0u);
    CHECK(snapshot.direction == 1);
}

static void test_saturation_diagnostic_counters_do_not_wrap(void) {
    pd_mode_axis_state_t state = {
        .saturated_report_count = UINT32_MAX,
        .discarded_tap_count    = UINT32_MAX - 1u,
    };
    uint8_t remaining_budget = NOAH_PD_MODE_MAX_TAPS_PER_TICK;

    test_reset_stubs();
    (void)pd_mode_axis_emit(&state, INT16_MAX, KC_AUDIO_VOL_DOWN, KC_AUDIO_VOL_UP, 60u, &remaining_budget, pd_mode_tap_code);

    CHECK(state.saturated_report_count == UINT32_MAX);
    CHECK(state.discarded_tap_count == UINT32_MAX);
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_AUDIO_VOL_DOWN);
}

static void test_direction_reversal_discards_old_debt_before_new_output(void) {
    pd_mode_axis_debug_snapshot_t snapshot;

    test_reset_stubs();
    (void)handle_volume_mode((report_mouse_t){.y = INT16_MAX});

    test_clear_logs();
    (void)handle_volume_mode((report_mouse_t){.y = -60});
    test_assert_synthetic_calls(1u, KC_AUDIO_VOL_UP);

    pd_mode_volume_debug_snapshot(&snapshot);
    CHECK(snapshot.accumulated_motion == 0);
    CHECK(snapshot.pending_tap_count == 0u);
    CHECK(snapshot.direction == -1);

    test_clear_logs();
    (void)handle_volume_mode((report_mouse_t){0});
    CHECK(synthetic_tap_call_count == 0u);
}

static void test_mode_reset_clears_backlog_and_diagnostics(void) {
    pd_mode_axis_debug_snapshot_t snapshot;

    test_reset_stubs();
    (void)handle_volume_mode((report_mouse_t){.y = INT16_MAX});
    reset_volume_mode();

    pd_mode_volume_debug_snapshot(&snapshot);
    CHECK(snapshot.accumulated_motion == 0);
    CHECK(snapshot.pending_tap_count == 0u);
    CHECK(snapshot.saturated_report_count == 0u);
    CHECK(snapshot.discarded_tap_count == 0u);
    CHECK(snapshot.maximum_backlog_taps == 0u);
    CHECK(snapshot.maximum_taps_emitted == 0u);
    CHECK(snapshot.direction == 0);

    test_clear_logs();
    (void)handle_volume_mode((report_mouse_t){0});
    CHECK(synthetic_tap_call_count == 0u);
}

static void test_arrow_extreme_magnitudes_select_exact_dominant_axis(void) {
    test_reset_stubs();

    (void)handle_arrow_mode((report_mouse_t){
        .x = INT16_MIN,
        .y = INT16_MAX,
    });

    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_LEFT);

    test_reset_stubs();

    (void)handle_arrow_mode((report_mouse_t){
        .x = INT16_MAX,
        .y = INT16_MIN,
    });

    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_UP);

    test_reset_stubs();
    (void)handle_arrow_mode((report_mouse_t){.x = INT16_MIN, .y = INT16_MIN});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_LEFT);

    test_reset_stubs();
    (void)handle_arrow_mode((report_mouse_t){.y = 50});
    test_clear_logs();
    (void)handle_arrow_mode((report_mouse_t){.x = INT16_MIN, .y = INT16_MIN});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_UP);
}

static void test_arrow_x_to_y_switch_cancels_horizontal_debt(void) {
    pd_mode_arrow_debug_snapshot_t snapshot;

    test_reset_stubs();

    // Sub-threshold horizontal debt while X is dominant.
    (void)handle_arrow_mode((report_mouse_t){.x = 39});
    CHECK(synthetic_tap_call_count == 0u);

    // A purely vertical report carries no horizontal input to notice, so the
    // transition itself has to clear the horizontal state.
    test_clear_logs();
    (void)handle_arrow_mode((report_mouse_t){.y = 50});
    test_assert_synthetic_calls(1u, KC_DOWN);

    pd_mode_arrow_debug_snapshot(&snapshot);
    CHECK(!snapshot.selected_axis_is_horizontal);
    CHECK(snapshot.horizontal.accumulated_motion == 0);
    CHECK(snapshot.horizontal.pending_tap_count == 0u);
    CHECK(snapshot.horizontal.direction == 0);

    // Returning to X starts from zero instead of finishing the old debt.
    test_clear_logs();
    (void)handle_arrow_mode((report_mouse_t){.x = 1});
    CHECK(synthetic_tap_call_count == 0u);

    pd_mode_arrow_debug_snapshot(&snapshot);
    CHECK(snapshot.selected_axis_is_horizontal);
    CHECK(snapshot.horizontal.accumulated_motion == 1);
    CHECK(snapshot.vertical.accumulated_motion == 0);

    (void)handle_arrow_mode((report_mouse_t){0});
    CHECK(synthetic_tap_call_count == 0u);
}

static void test_arrow_y_to_x_switch_cancels_vertical_debt(void) {
    pd_mode_arrow_debug_snapshot_t snapshot;

    test_reset_stubs();

    (void)handle_arrow_mode((report_mouse_t){.y = 49});
    CHECK(synthetic_tap_call_count == 0u);

    test_clear_logs();
    (void)handle_arrow_mode((report_mouse_t){.x = 40});
    test_assert_synthetic_calls(1u, KC_RIGHT);

    pd_mode_arrow_debug_snapshot(&snapshot);
    CHECK(snapshot.selected_axis_is_horizontal);
    CHECK(snapshot.vertical.accumulated_motion == 0);
    CHECK(snapshot.vertical.pending_tap_count == 0u);
    CHECK(snapshot.vertical.direction == 0);

    test_clear_logs();
    (void)handle_arrow_mode((report_mouse_t){.y = 1});
    CHECK(synthetic_tap_call_count == 0u);

    pd_mode_arrow_debug_snapshot(&snapshot);
    CHECK(!snapshot.selected_axis_is_horizontal);
    CHECK(snapshot.vertical.accumulated_motion == 1);
    CHECK(snapshot.horizontal.accumulated_motion == 0);

    (void)handle_arrow_mode((report_mouse_t){0});
    CHECK(synthetic_tap_call_count == 0u);
}

static void test_arrow_axis_switch_drops_bounded_backlog(void) {
    pd_mode_arrow_debug_snapshot_t snapshot;

    test_reset_stubs();

    // Fill the horizontal backlog, then leave the axis on a report with no
    // horizontal input at all.
    (void)handle_arrow_mode((report_mouse_t){.x = INT16_MAX});
    test_assert_synthetic_calls(NOAH_PD_MODE_MAX_TAPS_PER_TICK, KC_RIGHT);

    test_clear_logs();
    (void)handle_arrow_mode((report_mouse_t){.y = 50});
    test_assert_synthetic_calls(1u, KC_DOWN);

    pd_mode_arrow_debug_snapshot(&snapshot);
    CHECK(snapshot.horizontal.accumulated_motion == 0);
    CHECK(snapshot.horizontal.pending_tap_count == 0u);

    test_clear_logs();
    (void)handle_arrow_mode((report_mouse_t){.x = 1});
    CHECK(synthetic_tap_call_count == 0u);
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
    test_dragscroll_lock_timeout_boundary_uses_prior_motion_age();
    test_dragscroll_buffer_timeout_boundary_preserves_then_expires_residual();
    test_dragscroll_no_motion_expires_stale_state();
    test_dragscroll_first_post_expiry_report_can_emit_new_axis();
    test_dragscroll_rate_limit_boundary_is_inclusive();
    test_dragscroll_prior_motion_age_is_wrap_safe();
    test_dragscroll_uses_one_timer_sample_per_invocation();
    test_dragscroll_uses_different_horizontal_and_vertical_divisors();
    test_dragscroll_cross_axis_decay_prevents_residual_leakage();
    test_dragscroll_extreme_input_saturates_instead_of_wrapping();
    test_dragscroll_reverse_axis_extremes_saturate_in_both_directions();
    test_dragscroll_saturated_buffer_expires_after_pause();
    test_pinch_reuse_shares_saturating_dragscroll_accumulator();
    test_dragscroll_reset_clears_buffers_and_lock_state();
    test_volume_mode_emits_discrete_steps_and_resets_on_direction_change();
    test_brightness_mode_emits_discrete_steps_and_resets_on_direction_change();
    test_zoom_mode_emits_discrete_steps_and_resets_on_direction_change();
    test_maximum_volume_report_respects_per_tick_budget();
    test_maximum_reports_are_bounded_in_every_discrete_mode();
    test_zero_motion_drains_bounded_backlog_and_preserves_residual();
    test_sustained_maximum_input_stays_bounded_and_observable();
    test_saturation_diagnostic_counters_do_not_wrap();
    test_direction_reversal_discards_old_debt_before_new_output();
    test_mode_reset_clears_backlog_and_diagnostics();
    test_arrow_extreme_magnitudes_select_exact_dominant_axis();
    test_arrow_x_to_y_switch_cancels_horizontal_debt();
    test_arrow_y_to_x_switch_cancels_vertical_debt();
    test_arrow_axis_switch_drops_bounded_backlog();
    test_horizontal_arrow_tap_preserves_mod_state();
    test_vertical_arrow_tap_masks_alt_and_restores_mod_state();
    test_arrow_mode_selection_button_holds_and_releases_shift();
    test_arrow_mode_copy_shortcut_suspends_ambient_mods();

    puts("pd_mode_handlers host tests passed");
    return 0;
}
