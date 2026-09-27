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

static void test_dragscroll_saturating_accumulator(void) {
    report_mouse_t report;

    // Every configured scroll slot uses the shared residual cap.
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

#ifdef NOAH_PD_PROFILE_ENABLE
#include "users/noah/lib/pointing/modes/pd_mode_configured.h"
#include "pd_mode_engine_fixture.h"
#include "users/noah/lib/profile/schema/profile_pd_v1.h"
static uint8_t engine_mod_owners[8];
void keyboard_mod_ownership_register(uint16_t key) {
    CHECK(key >= KC_LEFT_CTRL && key <= KC_RIGHT_GUI);
    engine_mod_owners[key - KC_LEFT_CTRL]++;
    add_mods(MOD_BIT(key));
}
void keyboard_mod_ownership_unregister(uint16_t key) {
    CHECK(engine_mod_owners[key - KC_LEFT_CTRL]);
    if (!--engine_mod_owners[key - KC_LEFT_CTRL]) del_mods(MOD_BIT(key));
}
uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    uint8_t result = 0;
    for (uint8_t bit = 0; bit < 8; bit++) if (engine_mod_owners[bit]) result |= (1u << bit);
    return result & mods;
}
// The directional engine, on the Arrow slot (left/right/up/down arrows) with
// both thresholds at 10 so one step is one report of 10 counts.
static void directional_slot(uint8_t slot[96], uint8_t axis, uint8_t empty_policy, bool diagonals) {
    memcpy(slot, pd_engine_fixture + 8 + 4 * 96, 96);
    slot[3]  = axis;
    slot[32] = 10; slot[33] = 0;
    slot[34] = 10; slot[35] = 0;
    memset(slot + 70, 0, 20);
    if (diagonals) {
        slot[70] = KC_HOME; // up-left
        slot[74] = KC_PGUP; // up-right
        slot[78] = KC_END;  // down-left
        slot[82] = KC_PGDN; // down-right
    }
    slot[86] = axis == NOAH_PD_AXIS_EIGHT ? empty_policy : 0;
}

// The engine's clock for these tests; test_clear_logs() resets the fixture's.
static uint32_t directional_clock;

// One report, 8 ms after the last, and the taps it produced.
static void directional_move(int16_t x, int16_t y) {
    test_clear_logs();
    directional_clock += 8u;
    fake_time32 = directional_clock;
    CHECK(noah_pd_engine_motion((report_mouse_t){.x = x, .y = y}).x == 0);
}

static uint16_t tapped(uint8_t index) {
    return index < synthetic_tap_call_count ? synthetic_tap_calls[index].keycode : KC_NO;
}

// Taps from a run of identical reports, all expected to be `keycode`.
static uint8_t directional_run(int16_t x, int16_t y, uint8_t reports, uint16_t keycode) {
    uint8_t total = 0;
    for (uint8_t i = 0; i < reports; i++) {
        directional_move(x, y);
        for (uint8_t tap = 0; tap < synthetic_tap_call_count; tap++) {
            if (tapped(tap) != keycode) fprintf(stderr, "run (%d,%d) report %u: tapped 0x%04x, expected 0x%04x\n", x, y, (unsigned)i, (unsigned)tapped(tap), (unsigned)keycode);
            CHECK(tapped(tap) == keycode);
        }
        total += synthetic_tap_call_count;
    }
    return total;
}

static void test_configured_eight_directions(void) {
    uint8_t slot[96];

    directional_slot(slot, NOAH_PD_AXIS_EIGHT, NOAH_PD_EMPTY_DIRECTION_NEAREST, true);
    test_reset_stubs();
    directional_clock = 1000u;
    noah_pd_engine_enter(slot);

    // A diagonal move gives diagonal taps only: no straight taps in between,
    // even when it leans off 45 degrees (27 here) or wobbles.
    CHECK(directional_run(10, 5, 6, KC_PGDN) >= 5);
    CHECK(directional_run(10, 9, 1, KC_PGDN) >= 1);
    CHECK(directional_run(10, 3, 1, KC_PGDN) <= 2); // 17 degrees for one report: the hold rides it out

    // A straight move wobbling up to 35 degrees stays straight.
    directional_clock += 500u; // a pause releases the hold
    directional_move(10, 0);
    CHECK(synthetic_tap_call_count == 1 && tapped(0) == KC_RIGHT);
    const int16_t wobble[] = {3, 6, 2, 7, 4, 1, 6};
    for (uint8_t i = 0; i < sizeof(wobble) / sizeof(wobble[0]); i++) {
        directional_move(10, wobble[i]);
        CHECK(synthetic_tap_call_count == 1 && tapped(0) == KC_RIGHT);
    }

    // Turning from right to down goes through the diagonal without firing it.
    uint8_t diagonal = 0, down = 0;
    for (uint8_t i = 0; i < 6; i++) {
        directional_move(0, 10);
        for (uint8_t tap = 0; tap < synthetic_tap_call_count; tap++) {
            if (tapped(tap) == KC_PGDN) diagonal++;
            if (tapped(tap) == KC_DOWN) down++;
        }
    }
    CHECK(diagonal == 0 && down >= 4);

    // A reversal switches at once.
    directional_move(0, -10);
    CHECK(synthetic_tap_call_count == 1 && tapped(0) == KC_UP);

    // Every diagonal, one step each.
    const struct { int16_t x, y; uint16_t key; } diagonals[] = {{-10, -10, KC_HOME}, {10, -10, KC_PGUP}, {-10, 10, KC_END}, {10, 10, KC_PGDN}};
    for (uint8_t i = 0; i < 4; i++) {
        directional_clock += 500u;
        CHECK(directional_run(diagonals[i].x, diagonals[i].y, 4, diagonals[i].key) >= 3);
    }

    // A long move is capped per tick like every discrete mode.
    directional_clock += 500u;
    directional_move(10 * 10, 0);
    CHECK(synthetic_tap_call_count == NOAH_PD_MODE_MAX_TAPS_PER_TICK);

    // Empty diagonals. Nearest: the quadrant splits at 45 degrees, as in
    // dominant axis, so a 40-degree move is horizontal and a 50-degree one
    // vertical.
    directional_slot(slot, NOAH_PD_AXIS_EIGHT, NOAH_PD_EMPTY_DIRECTION_NEAREST, false);
    noah_pd_engine_enter(slot);
    CHECK(directional_run(12, -10, 4, KC_RIGHT) >= 3);
    directional_clock += 500u;
    CHECK(directional_run(10, -12, 4, KC_UP) >= 3);
    // Both: each diagonal step taps the two straight directions, the one the
    // movement leans toward first.
    directional_slot(slot, NOAH_PD_AXIS_EIGHT, NOAH_PD_EMPTY_DIRECTION_BOTH, false);
    noah_pd_engine_enter(slot);
    const struct { int16_t x, y; uint16_t lean, other; } both_leans[] = {{10, -8, KC_RIGHT, KC_UP}, {8, -10, KC_UP, KC_RIGHT}};
    for (uint8_t lean = 0; lean < 2; lean++) {
        uint8_t both = 0;
        directional_clock += 500u;
        for (uint8_t i = 0; i < 4; i++) {
            directional_move(both_leans[lean].x, both_leans[lean].y);
            CHECK(synthetic_tap_call_count % 2u == 0u);
            for (uint8_t tap = 0; tap < synthetic_tap_call_count; tap += 2) CHECK(tapped(tap) == both_leans[lean].lean && tapped(tap + 1u) == both_leans[lean].other);
            both += synthetic_tap_call_count;
        }
        CHECK(both >= 4);
    }
    // Nothing: the diagonal is a dead zone.
    directional_slot(slot, NOAH_PD_AXIS_EIGHT, NOAH_PD_EMPTY_DIRECTION_NOTHING, false);
    noah_pd_engine_enter(slot);
    CHECK(directional_run(10, -10, 4, KC_NO) == 0);
    noah_pd_engine_exit();
}

// Every directional mode is the engine; the policy for an empty direction
// applies to straight directions too, and a single axis counts only itself.
static void test_configured_empty_directions_and_single_axes(void) {
    uint8_t slot[96];

    // Dominant axis with "left" empty. Nearest: its share goes to up and down,
    // so a move leaning up-left taps up, at the rate of its upward part.
    directional_slot(slot, NOAH_PD_AXIS_DOMINANT, 0, false);
    slot[36] = 0; // left
    slot[86] = NOAH_PD_EMPTY_DIRECTION_NEAREST;
    test_reset_stubs();
    directional_clock = 1000u;
    noah_pd_engine_enter(slot);
    uint8_t up = directional_run(-10, -5, 8, KC_UP);
    CHECK(up >= 3 && up <= 5);
    // Nothing: moving left is a dead zone.
    slot[86] = NOAH_PD_EMPTY_DIRECTION_NOTHING;
    noah_pd_engine_enter(slot);
    CHECK(directional_run(-10, -3, 6, KC_NO) == 0);
    // Without diagonals a straight direction has no compass neighbours, so
    // "both" acts as nearest.
    slot[86] = NOAH_PD_EMPTY_DIRECTION_BOTH;
    noah_pd_engine_enter(slot);
    CHECK(directional_run(-10, -5, 8, KC_UP) >= 3);
    // In eight directions an empty straight direction sends its two
    // diagonals, the one the movement leans toward first: left leaning up taps
    // up-left then down-left, left leaning down the other way round.
    directional_slot(slot, NOAH_PD_AXIS_EIGHT, NOAH_PD_EMPTY_DIRECTION_BOTH, true);
    slot[36] = 0; // left
    noah_pd_engine_enter(slot);
    const struct { int16_t y; uint16_t lean, other; } leans[] = {{-2, KC_HOME, KC_END}, {2, KC_END, KC_HOME}};
    for (uint8_t lean = 0; lean < 2; lean++) {
        uint8_t pairs = 0;
        directional_clock += 500u;
        for (uint8_t i = 0; i < 4; i++) {
            directional_move(-10, leans[lean].y);
            CHECK(synthetic_tap_call_count % 2u == 0u);
            for (uint8_t tap = 0; tap < synthetic_tap_call_count; tap += 2) CHECK(tapped(tap) == leans[lean].lean && tapped(tap + 1u) == leans[lean].other);
            pairs += synthetic_tap_call_count / 2u;
        }
        CHECK(pairs >= 3);
    }

    // Horizontal only counts the horizontal part of any move.
    directional_slot(slot, NOAH_PD_AXIS_HORIZONTAL, 0, false);
    slot[34] = slot[35] = 0;
    memset(slot + 44, 0, 8); // up, down
    noah_pd_engine_enter(slot);
    CHECK(directional_run(3, 10, 10, KC_RIGHT) == 3);
    CHECK(directional_run(-10, 10, 2, KC_LEFT) == 2);
    noah_pd_engine_exit();
}

// Dominant axis is the same engine with four directions.
static void test_configured_dominant_axis(void) {
    uint8_t slot[96];

    directional_slot(slot, NOAH_PD_AXIS_DOMINANT, 0, true); // diagonal slots are ignored
    test_reset_stubs();
    directional_clock = 1000u;
    noah_pd_engine_enter(slot);
    CHECK(directional_run(10, 0, 3, KC_RIGHT) == 3);
    // Held right, a move drifting up to 50 degrees stays right...
    CHECK(directional_run(10, 11, 4, KC_RIGHT) >= 3);
    // ...and a clearly vertical one switches.
    CHECK(directional_run(2, 10, 5, KC_DOWN) >= 3);
    // Reversal is immediate on either axis; a right-angle turn takes a report
    // longer, because the heading has to leave the held direction's range.
    directional_move(0, -10);
    CHECK(synthetic_tap_call_count == 1 && tapped(0) == KC_UP);
    directional_move(-10, -2);
    CHECK(synthetic_tap_call_count == 0);
    CHECK(directional_run(-10, -2, 3, KC_LEFT) >= 2);
    directional_move(10, 0);
    CHECK(synthetic_tap_call_count == 1 && tapped(0) == KC_RIGHT);
    // Small moves add up along the held direction, even across a pause.
    directional_move(4, 0);
    CHECK(synthetic_tap_call_count == 0);
    directional_clock += 500u;
    directional_move(6, 0);
    CHECK(synthetic_tap_call_count == 1 && tapped(0) == KC_RIGHT);
    // Each axis counts in its own threshold.
    slot[34] = 40;
    noah_pd_engine_enter(slot);
    CHECK(directional_run(0, 20, 4, KC_DOWN) == 2);
    noah_pd_engine_exit();
}

static void test_configured_release_ownership_and_custom_slot(void) {
    uint8_t slot[96];
    memcpy(slot, pd_engine_fixture + 8 + 4 * 96, 96);
    slot[0] = 7; // Identity/name do not select Arrow's behavior.
    test_reset_stubs();
    noah_pd_engine_enter(slot);
    keyrecord_t first = test_record(true), second = test_record(true);
    second.event.key.col++;
    CHECK(noah_pd_engine_key(MS_BTN1, &first));
    CHECK(noah_pd_engine_key(MS_BTN1, &second));
    CHECK(engine_mod_owners[5] == 2);
    first.event.pressed = false;
    CHECK(noah_pd_engine_key(MS_BTN1, &first));
    CHECK(engine_mod_owners[5] == 1 && (fake_mods & MOD_BIT(KC_RIGHT_SHIFT)));
    // Switching releases old owned output but retains the old release route.
    noah_pd_engine_enter(slot);
    CHECK(engine_mod_owners[5] == 0 && noah_pd_engine_pending_release());
    first.event.pressed = true;
    CHECK(noah_pd_engine_key(MS_BTN1, &first));
    second.event.pressed = false;
    CHECK(noah_pd_engine_key(MS_BTN1, &second));
    CHECK(engine_mod_owners[5] == 1); // Late old release cannot release new Shift.
    first.event.pressed = false;
    CHECK(noah_pd_engine_key(MS_BTN1, &first));
    CHECK(!noah_pd_engine_pending_release());
    first.event.pressed = true;
    CHECK(noah_pd_engine_key(MS_BTN2, &first));
    CHECK(literal_tap_calls[0].keycode == G(KC_C));
    noah_pd_engine_exit();
    first.event.pressed = false;
    CHECK(noah_pd_engine_key(MS_BTN2, &first));
    CHECK(!noah_pd_engine_pending_release());
    memcpy(slot, pd_engine_fixture + 8 + 1 * 96, 96);
    slot[0] = 7;
    slot[34] = 1; slot[35] = 0;
    slot[44] = KC_A; slot[48] = KC_B;
    noah_pd_engine_enter(slot);
    test_clear_logs();
    noah_pd_engine_motion((report_mouse_t){.y = INT16_MAX});
    CHECK(synthetic_tap_call_count == 4 && synthetic_tap_calls[0].keycode == KC_B);
    test_clear_logs();
    noah_pd_engine_motion((report_mouse_t){.y = -1});
    CHECK(synthetic_tap_call_count == 1 && synthetic_tap_calls[0].keycode == KC_A);
    noah_pd_engine_exit();
    CHECK(noah_pd_engine_motion((report_mouse_t){.x = 13}).x == 13);
}
#endif

int main(void) {
    test_configured_release_ownership_and_custom_slot();
    test_configured_eight_directions();
    test_configured_dominant_axis();
    test_configured_empty_directions_and_single_axes();
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
    test_dragscroll_saturating_accumulator();
    test_dragscroll_reset_clears_buffers_and_lock_state();
    puts("pd_mode_handlers host tests passed");
    return 0;
}
