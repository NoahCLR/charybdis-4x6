#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/held_action.h"
#include "users/noah/lib/key/held_repeat.h"
#include "users/noah/lib/state/runtime_debug.h"

enum {
    TEST_ACTION = SAFE_RANGE + 0x10,
};

static uint16_t fake_time;
static uint8_t  fake_mods;
static uint8_t  fake_weak_mods;
static uint8_t  fake_oneshot_mods;
static uint8_t  fake_oneshot_locked_mods;
static uint8_t  send_keyboard_report_count;

layer_state_t layer_state;

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

static void test_reset_stubs(void) {
    fake_time                  = 1000;
    fake_mods                  = 0;
    fake_weak_mods             = 0;
    fake_oneshot_mods          = 0;
    fake_oneshot_locked_mods   = 0;
    send_keyboard_report_count = 0;
    layer_state                = 0;
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

void layer_on(uint8_t layer) {
    layer_state |= (layer_state_t)1u << layer;
}

void layer_off(uint8_t layer) {
    layer_state &= (layer_state_t) ~((layer_state_t)1u << layer);
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

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    (void)action;
    return NOAH_ACTION_HOLD_KIND_SHARED;
}

void noah_action_press(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

void noah_action_release(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

void action_dispatch(uint16_t action) {
    (void)action;
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    (void)action;
    (void)policy;
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

static void test_snapshot_captures_cross_subsystem_runtime_state(void) {
    noah_runtime_debug_snapshot_t snapshot;
    keypos_t                      layer_key  = test_keypos(1, 2);
    keypos_t                      action_key = test_keypos(3, 4);
    keypos_t                      repeat_key = test_keypos(5, 6);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    noah_runtime_shared_state.key.feedback.active                    = true;
    noah_runtime_shared_state.pd.local_active_flags                  = PD_MODE_VOLUME;
    noah_runtime_shared_state.pd.remote_display_active_flags         = PD_MODE_ARROW;
    noah_runtime_shared_state.key.slots_by_position[0].owner.keycode = KC_C;

    layer_ownership_set_lock_state(3, true);
    layer_ownership_momentary_press(layer_key, 2);
    noah_runtime_trace_emit(NOAH_TRACE_SPLIT_SYNC, NOAH_TRACE_SPLIT_SYNC_EVENT_SEND, PD_MODE_VOLUME, PD_MODE_ARROW);

    held_action_register(action_key, TEST_ACTION);
    held_repeat_start(repeat_key, TEST_ACTION, 25);

    keyboard_mod_ownership_register(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    noah_runtime_debug_snapshot(&snapshot);

    CHECK(snapshot.core.key.feedback.active);
    CHECK(snapshot.core.pd.local_active_flags == PD_MODE_VOLUME);
    CHECK(snapshot.core.pd.remote_display_active_flags == PD_MODE_ARROW);
    CHECK(snapshot.core.key.slots_by_position[0].owner.keycode == KC_C);

    CHECK(snapshot.layer_ownership.applied_layer_state == (((layer_state_t)1u << 2) | ((layer_state_t)1u << 3)));
    CHECK(snapshot.layer_ownership.locked_mask == ((layer_state_t)1u << 3));
    CHECK(snapshot.layer_ownership.momentary_refcounts[2] == 1);
    CHECK(snapshot.layer_ownership.bindings[0].active);
    CHECK(snapshot.layer_ownership.bindings[0].key_pos.row == layer_key.row);
    CHECK(snapshot.layer_ownership.bindings[0].key_pos.col == layer_key.col);

    CHECK(snapshot.held_actions.actions[0].active);
    CHECK(snapshot.held_actions.actions[0].key_pos.row == action_key.row);
    CHECK(snapshot.held_actions.actions[0].key_pos.col == action_key.col);
    CHECK(snapshot.held_actions.actions[0].action == TEST_ACTION);
    CHECK(snapshot.held_repeats.bindings[0].active);
    CHECK(snapshot.held_repeats.bindings[0].key_pos.row == repeat_key.row);
    CHECK(snapshot.held_repeats.bindings[0].key_pos.col == repeat_key.col);
    CHECK(snapshot.held_repeats.bindings[0].action == TEST_ACTION);
    CHECK(snapshot.held_repeats.bindings[0].interval_ms == 40);

    CHECK(snapshot.keyboard_mod_ownership.live_state.real == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(snapshot.keyboard_mod_ownership.live_state.weak == MOD_BIT(KC_RIGHT_ALT));
    CHECK(snapshot.keyboard_mod_ownership.live_state.oneshot == MOD_BIT(KC_LEFT_GUI));
    CHECK(snapshot.keyboard_mod_ownership.live_state.oneshot_locked == MOD_BIT(KC_RIGHT_GUI));
    CHECK(snapshot.keyboard_mod_ownership.managed_refcounts[1] == 1);

    CHECK(snapshot.trace.count == 3u);
    CHECK(!snapshot.trace.overflowed);
    CHECK(snapshot.trace.entries[0].kind == NOAH_TRACE_LAYER_OWNERSHIP);
    CHECK(snapshot.trace.entries[0].event == NOAH_TRACE_LAYER_OWNERSHIP_EVENT_LOCK);
    CHECK(snapshot.trace.entries[1].kind == NOAH_TRACE_LAYER_OWNERSHIP);
    CHECK(snapshot.trace.entries[1].event == NOAH_TRACE_LAYER_OWNERSHIP_EVENT_MOMENTARY_PRESS);
    CHECK(snapshot.trace.entries[2].kind == NOAH_TRACE_SPLIT_SYNC);
    CHECK(snapshot.trace.entries[2].event == NOAH_TRACE_SPLIT_SYNC_EVENT_SEND);
    CHECK(snapshot.trace.entries[2].a == PD_MODE_VOLUME);
    CHECK(snapshot.trace.entries[2].b == PD_MODE_ARROW);
}

static void test_reset_clears_all_runtime_surfaces(void) {
    noah_runtime_debug_snapshot_t snapshot;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    noah_runtime_shared_state.key.feedback.active                    = true;
    noah_runtime_shared_state.pd.local_locked_flags                  = PD_MODE_ARROW;
    noah_runtime_shared_state.pd.remote_display_locked_flags         = PD_MODE_VOLUME;
    noah_runtime_shared_state.key.slots_by_position[0].owner.keycode = KC_V;

    layer_ownership_set_lock_state(1, true);
    held_action_register(test_keypos(0, 1), TEST_ACTION);
    keyboard_mod_ownership_register(KC_LEFT_ALT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_CTRL);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    noah_runtime_reset_for_test();
    noah_runtime_debug_snapshot(&snapshot);

    CHECK(!snapshot.core.key.feedback.active);
    CHECK(snapshot.core.pd.local_active_flags == 0);
    CHECK(snapshot.core.pd.local_locked_flags == 0);
    CHECK(snapshot.core.pd.remote_display_active_flags == 0);
    CHECK(snapshot.core.pd.remote_display_locked_flags == 0);
    CHECK(snapshot.core.key.slots_by_position[0].owner.keycode == KC_NO);
    CHECK(snapshot.core.key.slots_by_position[0].timing.tap_hold_term == CUSTOM_TAP_HOLD_TERM);
    CHECK(snapshot.core.key.slots_by_position[0].semantic.preview_layer == UINT8_MAX);

    CHECK(snapshot.layer_ownership.applied_layer_state == 0);
    CHECK(snapshot.layer_ownership.locked_mask == 0);
    CHECK(snapshot.layer_ownership.momentary_refcounts[1] == 0);
    CHECK(!snapshot.layer_ownership.bindings[0].active);

    CHECK(!snapshot.held_actions.actions[0].active);
    CHECK(!snapshot.held_repeats.bindings[0].active);
    CHECK(snapshot.held_actions.modifier_refcounts[2] == 0);

    CHECK(snapshot.keyboard_mod_ownership.live_state.real == 0);
    CHECK(snapshot.keyboard_mod_ownership.live_state.weak == 0);
    CHECK(snapshot.keyboard_mod_ownership.live_state.oneshot == 0);
    CHECK(snapshot.keyboard_mod_ownership.live_state.oneshot_locked == 0);
    CHECK(snapshot.keyboard_mod_ownership.managed_refcounts[2] == 0);
    CHECK(snapshot.trace.count == 0u);
    CHECK(!snapshot.trace.overflowed);
    CHECK(send_keyboard_report_count >= 2);
}

int main(void) {
    test_snapshot_captures_cross_subsystem_runtime_state();
    test_reset_clears_all_runtime_surfaces();

    puts("runtime_debug host tests passed");
    return 0;
}
