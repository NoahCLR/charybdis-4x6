#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/runtime/key_runtime_process.h"
#include "users/noah/lib/key/runtime/key_runtime_state.h"
#include "users/noah/lib/key/runtime/key_runtime_transition.h"

static bool    suppress_default;
static bool    tracked_physical_event;
static bool    handled_key_stub_is_handled;
static bool    interrupted_active_key;
static bool    flushed_multi_tap;
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

static keypos_t test_active_slot_key_pos;

static void test_set_active_slot_key_pos(keypos_t key_pos) {
    test_active_slot_key_pos = key_pos;
}

static active_key_state_t *test_active_slot(void) {
    return key_runtime_slot_for_position(test_active_slot_key_pos);
}

#define active_key (*test_active_slot())

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
    noah_runtime_shared_state = (runtime_shared_state_t){0};
    test_set_active_slot_key_pos(test_keypos(0, 0));
    suppress_default               = false;
    tracked_physical_event         = false;
    handled_key_stub_is_handled    = false;
    interrupted_active_key         = false;
    flushed_multi_tap              = false;
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

bool is_layer_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

void key_runtime_transition_plan_init(key_runtime_transition_plan_t *plan) {
    *plan = (key_runtime_transition_plan_t){0};
}

void key_runtime_transition_execute_plan(const key_runtime_transition_plan_t *plan) {
    executed_transition_plan_count = plan->count;
}

void key_runtime_transition_flush_multi_tap(key_runtime_transition_plan_t *plan) {
    flushed_multi_tap = true;
    plan->count++;
}

void key_runtime_transition_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_transition_plan_t *plan) {
    (void)key_pos;
    interrupted_active_key = true;
    plan->count++;
}

void key_runtime_transition_interrupt_active_key_on_other_press(key_runtime_transition_plan_t *plan) {
    interrupted_active_key = true;
    plan->count++;
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

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    (void)action;
    (void)policy;
}

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    return (key_behavior_view_t){
        .keycode = keycode,
    };
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    (void)keycode;
    (void)tap_count;
    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    (void)keycode;
    (void)count;
    return false;
}

handled_key_view_t handled_key_lookup(uint16_t keycode) {
    uint16_t flags = handled_key_stub_is_handled ? HANDLED_KEY_FLAG_HANDLED : 0;

    return (handled_key_view_t){
        .flags = flags,
    };
}

handled_key_view_t handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count) {
    (void)tap_count;
    return handled_key_lookup(keycode);
}

bool handled_key_is_handled(handled_key_view_t key) {
    return (key.flags & HANDLED_KEY_FLAG_HANDLED) != 0;
}

bool handled_key_has_multi_tap(handled_key_view_t key) {
    (void)key;
    return false;
}

bool handled_key_is_momentary_layer(handled_key_view_t key) {
    (void)key;
    return false;
}

bool handled_key_is_layer_tap(handled_key_view_t key) {
    (void)key;
    return false;
}

uint8_t handled_key_layer(handled_key_view_t key) {
    (void)key;
    return UINT8_MAX;
}

pd_mode_mask_t handled_key_pd_mode(handled_key_view_t key) {
    (void)key;
    return 0;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    (void)mt;
    return (delayed_action_mods_t){0};
}

static void test_active_handled_release_bypasses_modifier_suppression(void) {
    keyrecord_t record = test_record(test_keypos(1, 2), false);

    test_reset_state();
    suppress_default = true;
    test_set_active_slot_key_pos(record.event.key);
    active_key.owner.keycode = KC_RIGHT_ALT;
    active_key.owner.key_pos = record.event.key;

    CHECK(key_runtime_preflight_record(KC_RIGHT_ALT, &record));
    CHECK(tracked_physical_event);
}

static void test_unrelated_release_stays_suppressed(void) {
    keyrecord_t record = test_record(test_keypos(1, 2), false);
    keypos_t    stored = test_keypos(1, 3);

    test_reset_state();
    suppress_default = true;
    test_set_active_slot_key_pos(stored);
    active_key.owner.keycode = KC_RIGHT_ALT;
    active_key.owner.key_pos = stored;

    CHECK(!key_runtime_preflight_record(KC_RIGHT_ALT, &record));
    CHECK(tracked_physical_event);
}

static void test_inactive_handled_release_bypasses_modifier_suppression(void) {
    keyrecord_t record = test_record(test_keypos(1, 2), false);
    keypos_t    stored = test_keypos(4, 4);

    test_reset_state();
    suppress_default            = true;
    handled_key_stub_is_handled = true;
    test_set_active_slot_key_pos(stored);
    active_key.owner.keycode = KC_LEFT_CTRL;
    active_key.owner.key_pos = stored;

    CHECK(key_runtime_preflight_record(KC_RIGHT_ALT, &record));
    CHECK(tracked_physical_event);
}

static void test_other_press_interrupts_active_key_through_transition_plan(void) {
    keyrecord_t record = test_record(test_keypos(2, 4), true);
    keypos_t    stored = test_keypos(2, 3);

    test_reset_state();
    test_set_active_slot_key_pos(stored);
    active_key.owner.keycode           = KC_RIGHT_ALT;
    active_key.owner.key_pos           = stored;
    active_key.lifecycle.phase         = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW;
    active_key.interaction.valid       = true;
    active_key.interaction.view        = (handled_key_view_t){
        .tap_action    = KC_NO,
        .hold_strategy = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK,
        .layer         = UINT8_MAX,
        .pd_mode       = 0,
        .flags         = HANDLED_KEY_FLAG_HANDLED | HANDLED_KEY_FLAG_FALLBACK_HOLD,
    };

    CHECK(key_runtime_preflight_record(KC_LEFT_CTRL, &record));
    CHECK(tracked_physical_event);
    CHECK(interrupted_active_key);
    CHECK(executed_transition_plan_count == 1);
}

static void test_handled_press_keeps_foreign_multi_tap_pending(void) {
    keyrecord_t record = test_record(test_keypos(3, 4), true);

    test_reset_state();
    handled_key_stub_is_handled                                         = true;
    key_runtime_slot_for_position(test_keypos(3, 3))->pending_multi_tap = (multi_tap_t){
        .keycode = KC_RIGHT_ALT,
        .key_pos = test_keypos(3, 3),
        .count   = 1,
    };

    CHECK(key_runtime_preflight_record(KC_LEFT_CTRL, &record));
    CHECK(!flushed_multi_tap);
    CHECK(executed_transition_plan_count == 0);
}

static void test_non_handled_press_flushes_foreign_multi_tap(void) {
    keyrecord_t record = test_record(test_keypos(3, 4), true);

    test_reset_state();
    key_runtime_slot_for_position(test_keypos(3, 3))->pending_multi_tap = (multi_tap_t){
        .keycode = KC_RIGHT_ALT,
        .key_pos = test_keypos(3, 3),
        .count   = 1,
    };

    CHECK(key_runtime_preflight_record(KC_LEFT_CTRL, &record));
    CHECK(flushed_multi_tap);
    CHECK(executed_transition_plan_count == 1);
}

int main(void) {
    test_active_handled_release_bypasses_modifier_suppression();
    test_unrelated_release_stays_suppressed();
    test_inactive_handled_release_bypasses_modifier_suppression();
    test_other_press_interrupts_active_key_through_transition_plan();
    test_handled_press_keeps_foreign_multi_tap_pending();
    test_non_handled_press_flushes_foreign_multi_tap();

    puts("key_runtime_preflight host tests passed");
    return 0;
}

#undef active_key
