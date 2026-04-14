#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/ownership/held_action.h"
#include "users/noah/lib/key/ownership/held_repeat.h"
#include "users/noah/lib/key/runtime/key_runtime_feedback.h"
#include "users/noah/lib/key/runtime/key_runtime_state.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/pointing/runtime/pd_mode_internal.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"
#include "host_handled_key_fixture.h"

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

static bool test_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

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

static handled_key_resolution_t test_handled_key_resolution(uint16_t keycode, uint8_t tap_count) {
    return (handled_key_resolution_t){
        .keycode          = keycode,
        .tap_count        = tap_count,
        .step             = {.tap = TAP_SENDS(keycode)},
        .tap_hold_term    = CUSTOM_TAP_HOLD_TERM,
        .longer_hold_term = CUSTOM_LONGER_HOLD_TERM,
        .multi_tap_term   = CUSTOM_MULTI_TAP_TERM,
        .layer            = UINT8_MAX,
        .pd_mode          = 0,
        .has_more_taps    = false,
        .flags            = HANDLED_KEY_FLAG_HANDLED,
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

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    (void)layer_num;
    (void)row;
    (void)column;
    return KC_TRNS;
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

handled_key_resolution_t handled_key_lookup(uint16_t keycode) {
    return test_handled_key_resolution(keycode, 1);
}

handled_key_resolution_t handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count) {
    return test_handled_key_resolution(keycode, tap_count);
}

hold_behavior_t handled_key_resolution_hold(handled_key_resolution_t key) {
    return key.step.hold;
}

hold_behavior_t handled_key_resolution_long_hold(handled_key_resolution_t key) {
    return key.step.long_hold;
}

key_runtime_slot_hold_strategy_t handled_key_resolution_hold_strategy(handled_key_resolution_t key) {
    (void)key;
    return KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
}

uint16_t handled_key_resolution_tap_action(handled_key_resolution_t key) {
    return key.step.tap.present ? key.step.tap.action : KC_NO;
}

uint8_t handled_key_resolution_tap_repeat_count(handled_key_resolution_t key) {
    return handled_key_resolution_tap_action(key) == KC_NO ? 0 : 1;
}

bool handled_key_resolution_tap_resolves_on_press(handled_key_resolution_t key) {
    (void)key;
    return false;
}

bool handled_key_resolution_uses_fallback_hold(handled_key_resolution_t key) {
    (void)key;
    return false;
}

bool handled_key_resolution_uses_implicit_hold(handled_key_resolution_t key) {
    (void)key;
    return false;
}

bool handled_key_resolution_has_multi_tap(handled_key_resolution_t key) {
    return (key.flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0;
}

bool handled_key_resolution_is_momentary_layer(handled_key_resolution_t key) {
    return (key.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0;
}

bool handled_key_resolution_is_layer_tap(handled_key_resolution_t key) {
    return (key.flags & HANDLED_KEY_FLAG_LAYER_TAP) != 0;
}

uint8_t handled_key_resolution_layer(handled_key_resolution_t key) {
    return key.layer;
}

pd_mode_mask_t handled_key_resolution_pd_mode(handled_key_resolution_t key) {
    return key.pd_mode;
}

handled_key_resolution_ctx_t handled_key_resolution_ctx_live(keypos_t key_pos) {
    return handled_key_resolution_ctx_make(key_pos, (layer_state_t)1u << 0);
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
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

static uint8_t test_pd_mode_index(pd_mode_mask_t mode) {
    switch (mode) {
        case PD_MODE_VOLUME:
            return PD_MODE_INDEX_VOLUME;
        case PD_MODE_ARROW:
            return PD_MODE_INDEX_ARROW;
        default:
            return PD_MODE_COUNT;
    }
}

const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {
    [PD_MODE_INDEX_VOLUME] =
        {
            .mode_flag   = PD_MODE_VOLUME,
            .keycode     = VOLUME_MODE,
            .lock_action = VOLUME_MODE_LOCK,
        },
    [PD_MODE_INDEX_ARROW] =
        {
            .mode_flag   = PD_MODE_ARROW,
            .keycode     = ARROW_MODE,
            .lock_action = ARROW_MODE_LOCK,
        },
};

const pd_mode_def_t *pd_mode_lookup(pd_mode_mask_t mode) {
    uint8_t index = test_pd_mode_index(mode);
    return index < PD_MODE_COUNT ? &pd_modes[index] : NULL;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    for (uint8_t index = 0; index < PD_MODE_COUNT; index++) {
        if (pd_modes[index].lock_action == action) {
            return &pd_modes[index];
        }
    }

    return NULL;
}

bool is_pd_mode_lock_action(uint16_t action) {
    return pd_mode_lock_action_lookup(action) != NULL;
}

bool pd_mode_has_trait(pd_mode_mask_t mode, pd_mode_traits_t trait) {
    const pd_mode_def_t *def = pd_mode_lookup(mode);
    return def != NULL && (def->traits & trait) == trait;
}

bool pd_any_active_mode_has_trait(pd_mode_traits_t trait) {
    return pd_mode_has_trait(pd_mode_local_active_snapshot(), trait);
}

bool is_keyboard_master(void) {
    return true;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    if (keycode == VOLUME_MODE) {
        return PD_MODE_VOLUME;
    }

    if (keycode == ARROW_MODE) {
        return PD_MODE_ARROW;
    }

    return 0;
}

void pd_mode_transition_activate(pd_mode_mask_t mode) {
    pd_mode_set(mode);
}

void pd_mode_transition_deactivate(pd_mode_mask_t mode) {
    pd_mode_clear(mode);
}

void pd_mode_transition_lock(pd_mode_mask_t mode) {
    pd_mode_set_locked(mode);
    pd_mode_set(mode);
}

void pd_mode_transition_unlock(pd_mode_mask_t mode) {
    pd_mode_clear_locked(mode);
    pd_mode_clear(mode);
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

static void test_stage_active_slot(uint16_t keycode, keypos_t key_pos) {
    active_key_state_t *slot = NULL;

    CHECK(key_pos.row == 0);
    CHECK(key_pos.col == 0);
    slot = key_runtime_slot_at(0);
    CHECK(slot != NULL);
    key_runtime_slot_track(slot, keycode, key_pos, host_key_runtime_slot_interaction_from_authored_resolution(test_handled_key_resolution(keycode, 1), handled_key_resolution_ctx_make(key_pos, (layer_state_t)1u << 0)), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
}

static void test_snapshot_captures_cross_subsystem_runtime_state(void) {
    noah_runtime_debug_snapshot_t snapshot;
    keypos_t                      key_pos;
    keypos_t                      active_key = test_keypos(0, 0);
    keypos_t                      pending_key = test_keypos(0, 1);
    keypos_t                      layer_key  = test_keypos(1, 2);
    keypos_t                      action_key = test_keypos(3, 4);
    keypos_t                      repeat_key = test_keypos(5, 6);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    key_feedback_pulse_arm(false);
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind = PD_MODE_COMMAND_ACTIVATE,
        .mode = PD_MODE_VOLUME,
    });
    pd_mode_apply_remote_snapshot(PD_MODE_ARROW, 0);
    test_stage_active_slot(KC_C, active_key);
    key_runtime_slot_begin_pending_multi_tap(key_runtime_slot_at(1), TEST_ACTION, pending_key, TEST_ACTION, 1, 120, 180, false);
    noah_runtime_trace_reset();

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

    CHECK(snapshot.key.feedback_active);
    CHECK(snapshot.pd.local_active_mode == PD_MODE_VOLUME);
    CHECK(snapshot.pd.remote_display_active_mode == PD_MODE_ARROW);
    CHECK(noah_runtime_debug_slot_owner_keycode(&snapshot, active_key) == KC_C);
    CHECK(noah_runtime_debug_slot_tap_action(&snapshot, active_key) == KC_C);
    CHECK(noah_runtime_debug_active_slot_count(&snapshot) == 1);
    CHECK(noah_runtime_debug_active_slot_key_pos(&snapshot, 0, &key_pos));
    CHECK(test_keypos_equal(key_pos, active_key));
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count(&snapshot) == 1);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_key_pos(&snapshot, 0, &key_pos));
    CHECK(test_keypos_equal(key_pos, pending_key));
    CHECK(!noah_runtime_debug_preview_owner_slot_key_pos(&snapshot, &key_pos));
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&snapshot, &key_pos));

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
    keypos_t                      active_key = test_keypos(0, 0);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    key_feedback_pulse_arm(true);
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind = PD_MODE_COMMAND_LOCK,
        .mode = PD_MODE_ARROW,
    });
    pd_mode_apply_remote_snapshot(PD_MODE_VOLUME, PD_MODE_VOLUME);
    test_stage_active_slot(KC_V, active_key);

    layer_ownership_set_lock_state(1, true);
    held_action_register(test_keypos(0, 1), TEST_ACTION);
    keyboard_mod_ownership_register(KC_LEFT_ALT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_CTRL);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    noah_runtime_reset_for_test();
    noah_runtime_debug_snapshot(&snapshot);

    CHECK(!snapshot.key.feedback_active);
    CHECK(snapshot.pd.local_active_mode == 0);
    CHECK(snapshot.pd.local_locked_mode == 0);
    CHECK(snapshot.pd.remote_display_active_mode == 0);
    CHECK(snapshot.pd.remote_display_locked_mode == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(&snapshot, active_key) == KC_NO);
    CHECK(noah_runtime_debug_slot_tap_action(&snapshot, active_key) == KC_NO);
    CHECK(noah_runtime_debug_active_slot_count(&snapshot) == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count(&snapshot) == 0);
    CHECK(!noah_runtime_debug_preview_owner_slot_key_pos(&snapshot, &active_key));
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&snapshot, &active_key));

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
