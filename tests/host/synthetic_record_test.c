#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/synthetic_record.h"

// Synthetic dispatch is what keeps engine-emitted output from being mistaken for
// a physical key event. The key runtime branches on noah_synthetic_record_active()
// in four places, including the guard that stops a synthetic emission from being
// credited as default-handler report ownership, so the depth counter and the
// non-matrix key position are both load-bearing contracts rather than details.

static uint8_t  observed_calls;
static bool     observed_active_during;
static bool     observed_pressed[4];
static keypos_t observed_key;
static uint16_t observed_keycode;
static uint8_t  observed_tap_count;
static bool     observed_nested_active;
static bool     nest_once;

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

static void test_reset(void) {
    observed_calls         = 0;
    observed_active_during = false;
    observed_key           = (keypos_t){0};
    observed_keycode       = KC_NO;
    observed_tap_count     = 0;
    observed_nested_active = false;
    nest_once              = false;
    for (uint8_t index = 0; index < 4u; index++) {
        observed_pressed[index] = false;
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (observed_calls < 4u) {
        observed_pressed[observed_calls] = record->event.pressed;
    }
    observed_calls++;
    observed_active_during = noah_synthetic_record_active();
    observed_key           = record->event.key;
    observed_keycode       = keycode;

    if (nest_once) {
        nest_once = false;
        (void)noah_dispatch_synthetic_record(KC_B, true);
        // The outer dispatch is still in flight, so the inner one unwinding must
        // not clear the flag.
        observed_nested_active = noah_synthetic_record_active();
    }

    return true;
}

bool process_record(keyrecord_t *record) {
    observed_calls++;
    observed_active_during = noah_synthetic_record_active();
    observed_key           = record->event.key;
#ifndef NO_ACTION_TAPPING
    observed_tap_count = record->tap.count;
#endif
    return true;
}

// A synthetic record must never name a real matrix position, or it would collide
// with the per-key runtime state of whatever key sits there.
static void test_synthetic_record_uses_a_non_matrix_key_position(void) {
    test_reset();

    CHECK(noah_dispatch_synthetic_record(KC_A, true));

    CHECK(observed_calls == 1u);
    CHECK(observed_keycode == KC_A);
    CHECK(observed_key.row >= MATRIX_ROWS);
    CHECK(observed_key.col >= MATRIX_COLS);
}

static void test_synthetic_dispatch_marks_and_restores_the_active_flag(void) {
    test_reset();

    CHECK(!noah_synthetic_record_active());

    CHECK(noah_dispatch_synthetic_record(KC_A, true));

    CHECK(observed_active_during);
    CHECK(!noah_synthetic_record_active());
}

static void test_nested_synthetic_dispatch_stays_active_until_the_outer_unwinds(void) {
    test_reset();
    nest_once = true;

    CHECK(noah_dispatch_synthetic_record(KC_A, true));

    CHECK(observed_calls == 2u);
    CHECK(observed_nested_active);
    CHECK(!noah_synthetic_record_active());
}

static void test_synthetic_tap_emits_press_then_release(void) {
    test_reset();

    noah_dispatch_synthetic_tap(KC_A);

    CHECK(observed_calls == 2u);
    CHECK(observed_pressed[0]);
    CHECK(!observed_pressed[1]);
    CHECK(!noah_synthetic_record_active());
}

static void test_synthetic_qmk_record_marks_active_and_carries_tap_count(void) {
    test_reset();

    noah_dispatch_synthetic_qmk_record(KC_A, true, 3u);

    CHECK(observed_calls == 1u);
    CHECK(observed_active_during);
    CHECK(observed_key.row >= MATRIX_ROWS);
    CHECK(observed_key.col >= MATRIX_COLS);
#ifndef NO_ACTION_TAPPING
    CHECK(observed_tap_count == 3u);
#endif
    CHECK(!noah_synthetic_record_active());
}

static void test_synthetic_qmk_tap_emits_press_then_release(void) {
    test_reset();

    noah_dispatch_synthetic_qmk_tap(KC_A);

    CHECK(observed_calls == 2u);
    CHECK(!noah_synthetic_record_active());
}

int main(void) {
    test_synthetic_record_uses_a_non_matrix_key_position();
    test_synthetic_dispatch_marks_and_restores_the_active_flag();
    test_nested_synthetic_dispatch_stays_active_until_the_outer_unwinds();
    test_synthetic_tap_emits_press_then_release();
    test_synthetic_qmk_record_marks_active_and_carries_tap_count();
    test_synthetic_qmk_tap_emits_press_then_release();

    puts("synthetic_record host tests passed");
    return 0;
}
