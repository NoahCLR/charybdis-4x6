#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/key/key_runtime_feedback.h"
#include "users/noah/lib/key/key_runtime_state.h"

static uint16_t fake_time;

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

static void test_reset_state(void) {
    noah_runtime_shared_state = (runtime_shared_state_t){0};
    fake_time                 = 0;
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

uint8_t get_mods(void) {
    return 0;
}

uint8_t get_weak_mods(void) {
    return 0;
}

uint8_t get_oneshot_mods(void) {
    return 0;
}

uint8_t get_oneshot_locked_mods(void) {
    return 0;
}

bool action_dispatch_is_layer_action(uint16_t action) {
    (void)action;
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

static void test_non_passthrough_held_action_flashes(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .keycode             = KC_RIGHT_ALT,
        .held_action_keycode = SAFE_RANGE + 1,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_hold_active(flags));
    CHECK(key_feedback_flags_level_flash(flags));
}

static void test_passthrough_modifier_hold_has_no_hold_feedback(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .keycode                       = KC_RIGHT_ALT,
        .held_action_keycode           = KC_RIGHT_ALT,
        .passthrough_modifier_pending  = true,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(flags == 0);
}

int main(void) {
    test_non_passthrough_held_action_flashes();
    test_passthrough_modifier_hold_has_no_hold_feedback();

    puts("key_runtime_feedback host tests passed");
    return 0;
}
