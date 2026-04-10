#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/key/key_runtime_process.h"
#include "users/noah/lib/key/key_runtime_state.h"
#include "users/noah/lib/key/key_runtime_transition.h"

static bool    suppress_default;
static bool    tracked_physical_event;
static bool    handled_key_is_handled;
static bool    interrupted_active_key;
static uint8_t executed_transition_plan_count;

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

static keyrecord_t test_record(keypos_t key_pos, bool pressed) {
    keyrecord_t record = {
        .event =
            {
                .key     = key_pos,
                .pressed = pressed,
            },
    };

    return record;
}

static void test_reset_state(void) {
    noah_runtime_shared_state      = (runtime_shared_state_t){0};
    suppress_default               = false;
    tracked_physical_event         = false;
    handled_key_is_handled         = false;
    interrupted_active_key         = false;
    executed_transition_plan_count = 0;
}

uint16_t timer_read(void) {
    return 0;
}

uint16_t timer_elapsed(uint16_t last) {
    (void)last;
    return 0;
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

void set_mods(uint8_t mods) {
    (void)mods;
}

void set_weak_mods(uint8_t mods) {
    (void)mods;
}

void set_oneshot_mods(uint8_t mods) {
    (void)mods;
}

void set_oneshot_locked_mods(uint8_t mods) {
    (void)mods;
}

void clear_mods(void) {}
void clear_weak_mods(void) {}
void clear_oneshot_mods(void) {}
void clear_oneshot_locked_mods(void) {}
void add_mods(uint8_t mods) {
    (void)mods;
}

void del_mods(uint8_t mods) {
    (void)mods;
}

void send_keyboard_report(void) {}

bool charybdis_get_pointer_dragscroll_enabled(void) {
    return false;
}

bool charybdis_get_pointer_sniping_enabled(void) {
    return false;
}

uint16_t charybdis_get_pointer_default_dpi(void) {
    return 0;
}

void charybdis_set_pointer_dragscroll_enabled(bool enabled) {
    (void)enabled;
}

void pointing_device_set_cpi(uint16_t cpi) {
    (void)cpi;
}

bool active_key_matches(uint16_t keycode, keypos_t key_pos) {
    return active_key.keycode == keycode && active_key.key_pos.row == key_pos.row && active_key.key_pos.col == key_pos.col;
}

bool is_layer_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

void active_key_reset(void) {
    active_key = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
}

void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired) {
    (void)tap_action;
    (void)hold;
    (void)long_hold;
    (void)tap_hold_term;
    (void)longer_hold_term;
    (void)multi_tap_term;
    (void)hold_fired;
    active_key.keycode = keycode;
    active_key.key_pos = key_pos;
}

bool multi_tap_active(const multi_tap_t *mt) {
    return mt->count > 0;
}

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan) {
    *plan = (key_runtime_transition_plan_t){0};
}

void key_runtime_transition_execute_plan(const key_runtime_transition_plan_t *plan) {
    executed_transition_plan_count = plan->count;
}

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan) {
    (void)plan;
}

void key_runtime_transition_interrupt_active_key_on_other_press(key_runtime_transition_plan_t *plan) {
    interrupted_active_key = true;
    plan->count++;
}

bool action_dispatch_is_layer_lock(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    tracked_physical_event = true;
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return suppress_default;
}

void action_dispatch(uint16_t action) {
    (void)action;
}

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    return (key_behavior_view_t){
        .keycode = keycode,
    };
}

handled_key_view_t handled_key_lookup(uint16_t keycode) {
    return (handled_key_view_t){
        .behavior =
            {
                .keycode = keycode,
                .handled = handled_key_is_handled,
            },
    };
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

static void test_active_handled_release_bypasses_modifier_suppression(void) {
    keyrecord_t record = test_record(test_keypos(1, 2), false);

    test_reset_state();
    suppress_default   = true;
    active_key.keycode = KC_RIGHT_ALT;
    active_key.key_pos = record.event.key;

    CHECK(key_runtime_preflight_record(KC_RIGHT_ALT, &record));
    CHECK(tracked_physical_event);
}

static void test_unrelated_release_stays_suppressed(void) {
    keyrecord_t record = test_record(test_keypos(1, 2), false);

    test_reset_state();
    suppress_default   = true;
    active_key.keycode = KC_RIGHT_ALT;
    active_key.key_pos = test_keypos(1, 3);

    CHECK(!key_runtime_preflight_record(KC_RIGHT_ALT, &record));
    CHECK(tracked_physical_event);
}

static void test_inactive_handled_release_bypasses_modifier_suppression(void) {
    keyrecord_t record = test_record(test_keypos(1, 2), false);

    test_reset_state();
    suppress_default       = true;
    handled_key_is_handled = true;
    active_key.keycode     = KC_LEFT_CTRL;
    active_key.key_pos     = test_keypos(4, 4);

    CHECK(key_runtime_preflight_record(KC_RIGHT_ALT, &record));
    CHECK(tracked_physical_event);
}

static void test_other_press_interrupts_active_key_through_transition_plan(void) {
    keyrecord_t record = test_record(test_keypos(2, 4), true);

    test_reset_state();
    active_key.keycode               = KC_RIGHT_ALT;
    active_key.key_pos               = test_keypos(2, 3);
    active_key.fallback_hold_pending = true;

    CHECK(key_runtime_preflight_record(KC_LEFT_CTRL, &record));
    CHECK(tracked_physical_event);
    CHECK(interrupted_active_key);
    CHECK(executed_transition_plan_count == 1);
}

int main(void) {
    test_active_handled_release_bypasses_modifier_suppression();
    test_unrelated_release_stays_suppressed();
    test_inactive_handled_release_bypasses_modifier_suppression();
    test_other_press_interrupts_active_key_through_transition_plan();

    puts("key_runtime_preflight host tests passed");
    return 0;
}
