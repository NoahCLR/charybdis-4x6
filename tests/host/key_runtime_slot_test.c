#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/key_runtime_slot_effect.h"
#include "users/noah/lib/key/key_runtime_slot_result.h"
#include "users/noah/lib/key/key_runtime_slot_step.h"
#include "users/noah/lib/key/key_runtime_state.h"

enum {
    TEST_MULTI_TAP_KEY = SAFE_RANGE + 0x40,
    TEST_ACTIVE_KEY    = SAFE_RANGE + 0x41,
    TEST_SINGLE_ACTION = SAFE_RANGE + 0x42,
    TEST_HOLD_ACTION   = SAFE_RANGE + 0x43,
    TEST_PD_MODE_KEY   = SAFE_RANGE + 0x44,
    TEST_LAYER_KEY     = SAFE_RANGE + 0x45,
    TEST_LAYER_LOCK    = SAFE_RANGE + 0x46,
};

static uint16_t fake_time;
static pd_mode_mask_t test_pd_mode;
static pd_mode_mask_t test_pd_locked_modes;

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

static active_key_state_t *test_primary_slot(void) {
    return key_runtime_slot_for_position(test_keypos(0, 0));
}

#define key_runtime_primary_slot() test_primary_slot()

static key_runtime_slot_result_t test_step_handled_press(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, handled_key_view_t key, bool active_held_action_survives_flush) {
    return key_runtime_slot_step(slot, (key_runtime_slot_event_t){
                                           .kind              = KEY_RUNTIME_SLOT_EVENT_HANDLED_PRESS,
                                           .data.handled_press = {
                                               .keycode                          = keycode,
                                               .key_pos                          = key_pos,
                                               .key                              = key,
                                               .active_held_action_survives_flush = active_held_action_survives_flush,
                                           },
                                       });
}

static key_runtime_slot_result_t test_step_handled_release(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, key_behavior_view_t behavior) {
    return key_runtime_slot_step(slot, (key_runtime_slot_event_t){
                                           .kind                = KEY_RUNTIME_SLOT_EVENT_HANDLED_RELEASE,
                                           .data.handled_release = {
                                               .keycode = keycode,
                                               .key_pos = key_pos,
                                               .key     = {
                                                   .behavior = behavior,
                                               },
                                           },
                                       });
}

static key_runtime_slot_result_t test_step_active_scan(active_key_state_t *slot) {
    return key_runtime_slot_step(slot, (key_runtime_slot_event_t){.kind = KEY_RUNTIME_SLOT_EVENT_ACTIVE_SCAN});
}

static key_runtime_slot_result_t test_step_pending_multi_tap_scan(active_key_state_t *slot) {
    return key_runtime_slot_step(slot, (key_runtime_slot_event_t){.kind = KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_SCAN});
}

static key_runtime_slot_result_t test_step_interrupt(active_key_state_t *slot, keypos_t other_key_pos) {
    return key_runtime_slot_step(slot, (key_runtime_slot_event_t){
                                           .kind           = KEY_RUNTIME_SLOT_EVENT_INTERRUPT,
                                           .data.interrupt = {
                                               .other_key_pos = other_key_pos,
                                           },
                                       });
}

static void test_reset_state(void) {
    fake_time                = 1000;
    test_pd_mode             = 0;
    test_pd_locked_modes     = 0;
    noah_runtime_shared_state = (runtime_shared_state_t){0};
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

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    if (keycode == TEST_MULTI_TAP_KEY && tap_count == 3) {
        return (key_behavior_step_t){
            .hold = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HOLD_ACTION),
        };
    }

    return key_behavior_step_none();
}

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    return (key_behavior_view_t){
        .keycode = keycode,
    };
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    return keycode == TEST_MULTI_TAP_KEY && count < 3;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    return keycode == TEST_PD_MODE_KEY ? test_pd_mode : 0;
}

bool pd_mode_locked(pd_mode_mask_t mode) {
    return (test_pd_locked_modes & mode) != 0;
}

bool action_dispatch_is_layer_lock(uint16_t action) {
    return action == TEST_LAYER_LOCK;
}

bool action_dispatch_is_qmk_behavior_keycode(uint16_t action) {
    (void)action;
    return false;
}

bool is_layer_key(uint16_t keycode) {
    return keycode == TEST_LAYER_KEY;
}

uint8_t behavior_get_layer(uint16_t keycode) {
    (void)keycode;
    return 3;
}

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    (void)action;
    return NOAH_ACTION_HOLD_KIND_SHARED;
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    (void)mt;
    return (delayed_action_mods_t){0};
}

static bool test_handled_key_uses_buffered_modifier_single_step(handled_key_view_t key) {
    if (key.behavior.single.tap.present || key.behavior.single.hold.present || key.behavior.single.long_hold.present || key.behavior.is_momentary_layer) {
        return false;
    }

    return key.behavior.has_multi_tap && key.behavior.keycode < SAFE_RANGE && false;
}

bool handled_key_uses_implicit_hold(handled_key_view_t key) {
    return pd_mode_for_keycode(key.behavior.keycode) != 0;
}

bool handled_key_uses_fallback_hold(handled_key_view_t key) {
    if (key.behavior.is_momentary_layer || key.behavior.keycode >= SAFE_RANGE) {
        return false;
    }

    if (action_dispatch_is_qmk_behavior_keycode(key.behavior.keycode)) {
        return false;
    }

    if (key.behavior.single.hold.present || key.behavior.single.long_hold.present) {
        return false;
    }

    return key.behavior.single.tap.present || key.behavior.has_multi_tap;
}

hold_behavior_t handled_key_single_hold(handled_key_view_t key) {
    if (key.behavior.single.hold.present) {
        return key.behavior.single.hold;
    }

    if (pd_mode_for_keycode(key.behavior.keycode)) {
        return (hold_behavior_t){
            .present = true,
            .action  = key.behavior.keycode,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        };
    }

    return hold_behavior_none();
}

hold_behavior_t handled_key_long_hold(handled_key_view_t key) {
    return key.behavior.single.long_hold;
}

key_runtime_slot_hold_strategy_t handled_key_hold_strategy(handled_key_view_t key) {
    if (handled_key_uses_implicit_hold(key)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
    }

    if (handled_key_uses_fallback_hold(key)) {
        return KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK;
    }

    return KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
}

uint16_t handled_key_tap_action(handled_key_view_t key) {
    if (key.behavior.single.tap.present) return key.behavior.single.tap.action;
    if (pd_mode_for_keycode(key.behavior.keycode)) return KC_NO;
    if (test_handled_key_uses_buffered_modifier_single_step(key)) return KC_NO;
    if (key.behavior.is_layer_tap) return QK_LAYER_TAP_GET_TAP_KEYCODE(key.behavior.keycode);
    if (key.behavior.is_momentary_layer) return KC_NO;
    if (key.behavior.keycode >= SAFE_RANGE) return KC_NO;
    return key.behavior.keycode;
}

uint16_t handled_key_tap_hold_term(handled_key_view_t key) {
    return key.behavior.tap_hold_term;
}

uint16_t handled_key_longer_hold_term(handled_key_view_t key) {
    return key.behavior.longer_hold_term;
}

uint16_t handled_key_multi_tap_term(handled_key_view_t key) {
    return key.behavior.multi_tap_term;
}

uint8_t handled_key_layer(handled_key_view_t key) {
    return key.behavior.is_momentary_layer ? behavior_get_layer(key.behavior.keycode) : UINT8_MAX;
}

pd_mode_mask_t handled_key_pd_mode(handled_key_view_t key) {
    return pd_mode_for_keycode(key.behavior.keycode);
}

bool handled_key_has_multi_tap(handled_key_view_t key) {
    return key.behavior.has_multi_tap;
}

bool handled_key_is_momentary_layer(handled_key_view_t key) {
    return key.behavior.is_momentary_layer;
}

bool handled_key_is_layer_tap(handled_key_view_t key) {
    return key.behavior.is_layer_tap;
}

void held_action_register(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

static void test_expect_dispatch_action(const key_runtime_slot_result_t *result, uint8_t index, uint16_t action) {
    CHECK(result->effects[index].kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(result->effects[index].data.action == action);
}

static void test_expect_held_register(const key_runtime_slot_result_t *result, uint8_t index, keypos_t key_pos, uint16_t action) {
    CHECK(result->effects[index].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(result->effects[index].data.held_action.key_pos.row == key_pos.row);
    CHECK(result->effects[index].data.held_action.key_pos.col == key_pos.col);
    CHECK(result->effects[index].data.held_action.action == action);
}

static void test_expect_held_unregister(const key_runtime_slot_result_t *result, uint8_t index, keypos_t key_pos, uint16_t action) {
    CHECK(result->effects[index].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER);
    CHECK(result->effects[index].data.held_action.key_pos.row == key_pos.row);
    CHECK(result->effects[index].data.held_action.key_pos.col == key_pos.col);
    CHECK(result->effects[index].data.held_action.action == action);
}

static void test_expect_release_owned_state(const key_runtime_slot_result_t *result, uint8_t index, keypos_t key_pos) {
    CHECK(result->effects[index].kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(result->effects[index].data.key_pos.row == key_pos.row);
    CHECK(result->effects[index].data.key_pos.col == key_pos.col);
}

static void test_expect_repeat_start(const key_runtime_slot_result_t *result, uint8_t index, keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    CHECK(result->effects[index].kind == KEY_RUNTIME_EFFECT_REPEAT_START);
    CHECK(result->effects[index].data.repeat.key_pos.row == key_pos.row);
    CHECK(result->effects[index].data.repeat.key_pos.col == key_pos.col);
    CHECK(result->effects[index].data.repeat.action == action);
    CHECK(result->effects[index].data.repeat.repeat_hz == repeat_hz);
}

static void test_expect_layer_press(const key_runtime_slot_result_t *result, uint8_t index, keypos_t key_pos, uint8_t layer) {
    CHECK(result->effects[index].kind == KEY_RUNTIME_EFFECT_LAYER_PRESS);
    CHECK(result->effects[index].data.layer_press.key_pos.row == key_pos.row);
    CHECK(result->effects[index].data.layer_press.key_pos.col == key_pos.col);
    CHECK(result->effects[index].data.layer_press.layer == layer);
}

static void test_expect_layer_release(const key_runtime_slot_result_t *result, uint8_t index, keypos_t key_pos) {
    CHECK(result->effects[index].kind == KEY_RUNTIME_EFFECT_LAYER_RELEASE);
    CHECK(result->effects[index].data.key_pos.row == key_pos.row);
    CHECK(result->effects[index].data.key_pos.col == key_pos.col);
}

static void test_expect_feedback_pulse(const key_runtime_slot_result_t *result, uint8_t index, bool long_hold_level) {
    CHECK(result->effects[index].kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(result->effects[index].data.long_hold_level == long_hold_level);
}

static void test_slot_pending_multi_tap_ownership_marks_slot_non_idle(void) {
    active_key_state_t *slot = key_runtime_slot_at(1);
    keypos_t            pos  = test_keypos(3, 4);

    test_reset_state();
    slot->pending_multi_tap = (multi_tap_t){
        .keycode = TEST_MULTI_TAP_KEY,
        .key_pos = pos,
        .count   = 1,
    };

    CHECK(!key_runtime_slot_idle(slot));
    CHECK(key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(key_runtime_slot_owns_key_position(slot, pos));
}

static void test_slot_pending_multi_tap_lifecycle_helpers(void) {
    active_key_state_t *slot = key_runtime_primary_slot();
    keypos_t            pos  = test_keypos(5, 0);

    test_reset_state();

    key_runtime_slot_begin_pending_multi_tap(slot, TEST_MULTI_TAP_KEY, pos, TEST_SINGLE_ACTION, 120, 150);
    CHECK(key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(slot->pending_multi_tap.count == 1);
    CHECK(!key_runtime_slot_pending_multi_tap_pending_hold(slot));
    CHECK(key_runtime_slot_pending_multi_tap_matches(slot, TEST_MULTI_TAP_KEY, pos));

    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(slot->pending_multi_tap.count == 2);
    CHECK(!key_runtime_slot_pending_multi_tap_pending_hold(slot));

    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(slot->pending_multi_tap.count == 3);
    CHECK(key_runtime_slot_pending_multi_tap_pending_hold(slot));
}

static void test_slot_track_preserves_pending_multi_tap(void) {
    active_key_state_t *slot = key_runtime_primary_slot();
    keypos_t            pos  = test_keypos(5, 1);

    test_reset_state();

    key_runtime_slot_begin_pending_multi_tap(slot, TEST_MULTI_TAP_KEY, pos, TEST_SINGLE_ACTION, 120, 150);
    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(key_runtime_slot_pending_multi_tap_pending_hold(slot));

    key_runtime_slot_track(slot, TEST_MULTI_TAP_KEY, pos, KC_NO, hold_behavior_none(), hold_behavior_none(), 120, 240, 150, KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW, KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT);

    CHECK(slot->owner.keycode == TEST_MULTI_TAP_KEY);
    CHECK(slot->pending_multi_tap.count == 3);
    CHECK(slot->pending_multi_tap.pending_hold);
    CHECK(slot->pending_multi_tap.keycode == TEST_MULTI_TAP_KEY);
}

static void test_resolve_pending_multi_tap_hold_clears_slot_owned_state(void) {
    active_key_state_t *slot = key_runtime_primary_slot();
    keypos_t            pos  = test_keypos(5, 2);
    uint8_t             repeat_count = 0;

    test_reset_state();

    key_runtime_slot_begin_pending_multi_tap(slot, TEST_MULTI_TAP_KEY, pos, TEST_SINGLE_ACTION, 120, 150);
    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(key_runtime_slot_advance_pending_multi_tap(slot, TEST_MULTI_TAP_KEY) == KC_NO);
    CHECK(key_runtime_slot_pending_multi_tap_pending_hold(slot));

    fake_time = (uint16_t)(fake_time + 130);

    CHECK(key_runtime_slot_resolve_pending_multi_tap_hold(slot, TEST_MULTI_TAP_KEY, &repeat_count) == TEST_HOLD_ACTION);
    CHECK(repeat_count == 1);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(key_runtime_slot_idle(slot));
}

static void test_take_active_release_starts_pending_multi_tap_chain(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;
    keypos_t                   pos = test_keypos(2, 3);

    test_reset_state();

    *slot = (active_key_state_t){
        .timer                  = (uint16_t)(fake_time - 50),
        .owner.keycode          = TEST_MULTI_TAP_KEY,
        .owner.key_pos          = pos,
        .binding.tap_action     = TEST_SINGLE_ACTION,
        .timing.tap_hold_term   = 120,
        .timing.multi_tap_term  = 150,
    };

    result = test_step_handled_release(
        slot,
        TEST_MULTI_TAP_KEY,
        pos,
        (key_behavior_view_t){
            .keycode        = TEST_MULTI_TAP_KEY,
            .has_multi_tap  = true,
            .tap_hold_term  = 120,
        });

    CHECK(result.handled);
    CHECK(result.count == 0);
    CHECK(slot->owner.keycode == KC_NO);
    CHECK(key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(slot->pending_multi_tap.keycode == TEST_MULTI_TAP_KEY);
    CHECK(slot->pending_multi_tap.key_pos.row == pos.row);
    CHECK(slot->pending_multi_tap.key_pos.col == pos.col);
    CHECK(slot->pending_multi_tap.count == 1);
    CHECK(slot->pending_multi_tap.single_action == TEST_SINGLE_ACTION);
}

static void test_take_active_release_quick_immediate_hold_becomes_tap(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;
    keypos_t                   pos = test_keypos(2, 1);

    test_reset_state();

    *slot = (active_key_state_t){
        .timer                         = (uint16_t)(fake_time - 50),
        .owner.keycode                 = TEST_ACTIVE_KEY,
        .owner.key_pos                 = pos,
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
        .lifecycle.held_action_keycode = TEST_HOLD_ACTION,
        .binding.tap_action            = TEST_SINGLE_ACTION,
        .timing.tap_hold_term          = 150,
        .binding.hold                  =
            {
                .present = true,
                .action  = TEST_HOLD_ACTION,
                .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
            },
    };

    result = test_step_handled_release(
        slot,
        TEST_ACTIVE_KEY,
        pos,
        (key_behavior_view_t){
            .keycode       = TEST_ACTIVE_KEY,
            .tap_hold_term = 150,
        });

    CHECK(result.handled);
    CHECK(result.count == 2);
    test_expect_release_owned_state(&result, 0, pos);
    test_expect_dispatch_action(&result, 1, TEST_SINGLE_ACTION);
    CHECK(key_runtime_slot_idle(slot));
}

static void test_take_active_release_maps_locked_pd_mode_tap(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;
    keypos_t                   pos = test_keypos(2, 4);

    test_reset_state();
    test_pd_mode         = PD_MODE_VOLUME;
    test_pd_locked_modes = PD_MODE_VOLUME;

    *slot = (active_key_state_t){
        .timer                              = (uint16_t)(fake_time - 50),
        .owner.keycode                      = TEST_PD_MODE_KEY,
        .owner.key_pos                      = pos,
        .timing.tap_hold_term               = 120,
        .lifecycle.pd_mode_was_locked_on_press = true,
    };

    result = test_step_handled_release(
        slot,
        TEST_PD_MODE_KEY,
        pos,
        (key_behavior_view_t){
            .keycode       = TEST_PD_MODE_KEY,
            .tap_hold_term = 120,
        });

    CHECK(result.handled);
    CHECK(result.count == 1);
    CHECK(result.effects[0].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_PD_MODE_LOCK_TAP);
    CHECK(result.effects[0].data.pd_mode == PD_MODE_VOLUME);
    CHECK(key_runtime_slot_idle(slot));
}

static void test_take_active_release_release_hold_pending_dispatches_release_hold_action(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;
    keypos_t                   pos = test_keypos(2, 5);

    test_reset_state();

    *slot = (active_key_state_t){
        .timer                    = (uint16_t)(fake_time - 180),
        .owner.keycode            = TEST_ACTIVE_KEY,
        .owner.key_pos            = pos,
        .lifecycle.phase          = KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING,
        .timing.tap_hold_term     = 120,
        .binding.hold             = TAP_ON_RELEASE_AFTER_HOLD(TEST_HOLD_ACTION),
    };

    result = test_step_handled_release(
        slot,
        TEST_ACTIVE_KEY,
        pos,
        (key_behavior_view_t){
            .keycode       = TEST_ACTIVE_KEY,
            .tap_hold_term = 120,
        });

    CHECK(result.handled);
    CHECK(result.count == 1);
    test_expect_dispatch_action(&result, 0, TEST_HOLD_ACTION);
    CHECK(key_runtime_slot_idle(slot));
}

static void test_take_active_release_hold_tier_active_dispatches_release_long_hold(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;
    keypos_t                   pos = test_keypos(2, 6);

    test_reset_state();

    *slot = (active_key_state_t){
        .timer                         = (uint16_t)(fake_time - 260),
        .owner.keycode                 = TEST_ACTIVE_KEY,
        .owner.key_pos                 = pos,
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE,
        .lifecycle.held_action_keycode = TEST_SINGLE_ACTION,
        .timing.tap_hold_term          = 120,
        .timing.longer_hold_term       = 240,
        .binding.hold                  = TAP_AT_HOLD_THRESHOLD(TEST_SINGLE_ACTION),
        .binding.long_hold             = TAP_ON_RELEASE_AFTER_HOLD(TEST_HOLD_ACTION),
    };

    result = test_step_handled_release(
        slot,
        TEST_ACTIVE_KEY,
        pos,
        (key_behavior_view_t){
            .keycode          = TEST_ACTIVE_KEY,
            .tap_hold_term    = 120,
            .longer_hold_term = 240,
        });

    CHECK(result.handled);
    CHECK(result.count == 2);
    test_expect_release_owned_state(&result, 0, pos);
    test_expect_dispatch_action(&result, 1, TEST_HOLD_ACTION);
    CHECK(key_runtime_slot_idle(slot));
}

static void test_take_active_scan_event_promotes_long_hold_after_longer_term(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;
    keypos_t                   pos = test_keypos(3, 1);

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode            = TEST_ACTIVE_KEY,
        .owner.key_pos            = pos,
        .lifecycle.phase          = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .timer                    = (uint16_t)(fake_time - 260),
        .timing.tap_hold_term     = 120,
        .timing.longer_hold_term  = 240,
        .binding.hold             = TAP_AT_HOLD_THRESHOLD(TEST_SINGLE_ACTION),
        .binding.long_hold        = TAP_AT_HOLD_THRESHOLD(TEST_HOLD_ACTION),
    };

    result = test_step_active_scan(slot);

    CHECK(result.handled);
    CHECK(result.count == 2);
    test_expect_dispatch_action(&result, 0, TEST_HOLD_ACTION);
    test_expect_feedback_pulse(&result, 1, true);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_active_scan_event_returns_commit_and_long_hold_requests(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode                 = TEST_ACTIVE_KEY,
        .owner.key_pos                 = test_keypos(3, 1),
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
        .timer                         = (uint16_t)(fake_time - 260),
        .timing.tap_hold_term          = 120,
        .timing.longer_hold_term       = 240,
        .lifecycle.held_action_keycode = TEST_HOLD_ACTION,
        .binding.hold                  =
            {
                .present = true,
                .action  = TEST_HOLD_ACTION,
                .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
            },
        .binding.long_hold             = TAP_AT_HOLD_THRESHOLD(TEST_SINGLE_ACTION),
    };

    result = test_step_active_scan(slot);

    CHECK(result.handled);
    CHECK(result.count == 4);
    test_expect_feedback_pulse(&result, 0, false);
    test_expect_release_owned_state(&result, 1, slot->owner.key_pos);
    test_expect_dispatch_action(&result, 2, TEST_SINGLE_ACTION);
    test_expect_feedback_pulse(&result, 3, true);
    CHECK(key_runtime_slot_hold_is_complete(slot));
    CHECK(slot->lifecycle.held_action_keycode == KC_NO);
}

static void test_take_active_scan_event_release_hold_pending_promotes_long_hold(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode            = TEST_ACTIVE_KEY,
        .owner.key_pos            = test_keypos(3, 5),
        .lifecycle.phase          = KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING,
        .timer                    = (uint16_t)(fake_time - 260),
        .timing.tap_hold_term     = 120,
        .timing.longer_hold_term  = 240,
        .binding.hold             = TAP_ON_RELEASE_AFTER_HOLD(TEST_SINGLE_ACTION),
        .binding.long_hold        = TAP_AT_HOLD_THRESHOLD(TEST_HOLD_ACTION),
    };

    result = test_step_active_scan(slot);

    CHECK(result.handled);
    CHECK(result.count == 2);
    test_expect_dispatch_action(&result, 0, TEST_HOLD_ACTION);
    test_expect_feedback_pulse(&result, 1, true);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_active_scan_event_hold_tier_active_promotes_long_hold_and_releases_owned_state(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode                 = TEST_ACTIVE_KEY,
        .owner.key_pos                 = test_keypos(3, 6),
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE,
        .timer                         = (uint16_t)(fake_time - 260),
        .timing.tap_hold_term          = 120,
        .timing.longer_hold_term       = 240,
        .lifecycle.held_action_keycode = TEST_SINGLE_ACTION,
        .binding.hold                  = TAP_AT_HOLD_THRESHOLD(TEST_SINGLE_ACTION),
        .binding.long_hold             = TAP_AT_HOLD_THRESHOLD(TEST_HOLD_ACTION),
    };

    result = test_step_active_scan(slot);

    CHECK(result.handled);
    CHECK(result.count == 3);
    test_expect_release_owned_state(&result, 0, slot->owner.key_pos);
    test_expect_dispatch_action(&result, 1, TEST_HOLD_ACTION);
    test_expect_feedback_pulse(&result, 2, true);
    CHECK(slot->lifecycle.held_action_keycode == KC_NO);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_active_scan_event_returns_fallback_hold_request(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode           = TEST_ACTIVE_KEY,
        .owner.key_pos           = test_keypos(3, 2),
        .lifecycle.phase         = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .timer                   = (uint16_t)(fake_time - 120),
        .timing.tap_hold_term    = 100,
        .lifecycle.hold_strategy = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK,
    };

    result = test_step_active_scan(slot);

    CHECK(result.handled);
    CHECK(result.count == 1);
    test_expect_held_register(&result, 0, slot->owner.key_pos, TEST_ACTIVE_KEY);
    CHECK(slot->lifecycle.held_action_keycode == TEST_ACTIVE_KEY);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_pending_multi_tap_scan_event_returns_long_hold_request(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;
    keypos_t                   pos = test_keypos(6, 0);

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode           = TEST_LAYER_KEY,
        .owner.key_pos           = pos,
        .lifecycle.phase         = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .timing.longer_hold_term = 240,
        .pending_multi_tap =
            {
                .keycode       = TEST_MULTI_TAP_KEY,
                .key_pos       = pos,
                .timer         = (uint16_t)(fake_time - 260),
                .count         = 2,
                .pending_hold  = true,
                .tap_hold_term = 120,
                .hold          = hold_behavior_none(),
                .long_hold     = TAP_AT_HOLD_THRESHOLD(TEST_LAYER_LOCK),
            },
    };

    result = test_step_pending_multi_tap_scan(slot);

    CHECK(result.handled);
    CHECK(result.count == 3);
    test_expect_layer_release(&result, 0, pos);
    test_expect_dispatch_action(&result, 1, TEST_LAYER_LOCK);
    test_expect_feedback_pulse(&result, 2, true);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(slot->binding.long_hold.action == TEST_LAYER_LOCK);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_pending_multi_tap_scan_event_returns_threshold_hold_request(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;
    keypos_t                   pos = test_keypos(6, 4);

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode         = TEST_ACTIVE_KEY,
        .owner.key_pos         = pos,
        .lifecycle.phase       = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .timing.tap_hold_term  = 120,
        .pending_multi_tap =
            {
                .keycode       = TEST_MULTI_TAP_KEY,
                .key_pos       = pos,
                .timer         = (uint16_t)(fake_time - 130),
                .count         = 2,
                .pending_hold  = true,
                .tap_hold_term = 120,
                .hold          = TAP_AT_HOLD_THRESHOLD(TEST_HOLD_ACTION),
                .long_hold     = hold_behavior_none(),
            },
    };

    result = test_step_pending_multi_tap_scan(slot);

    CHECK(result.handled);
    CHECK(result.count == 2);
    test_expect_dispatch_action(&result, 0, TEST_HOLD_ACTION);
    test_expect_feedback_pulse(&result, 1, false);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_pending_multi_tap_scan_event_flushes_expired_chain(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  result;

    test_reset_state();

    slot->pending_multi_tap = (multi_tap_t){
        .keycode        = TEST_MULTI_TAP_KEY,
        .key_pos        = test_keypos(6, 2),
        .timer          = (uint16_t)(fake_time - 200),
        .count          = 2,
        .single_action  = TEST_SINGLE_ACTION,
        .multi_tap_term = 150,
    };

    result = test_step_pending_multi_tap_scan(slot);

    CHECK(result.handled);
    CHECK(result.count == 1);
    CHECK(result.effects[0].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_DELAYED_ACTION);
    CHECK(result.effects[0].data.delayed_action.action == TEST_SINGLE_ACTION);
    CHECK(result.effects[0].data.delayed_action.repeat_count == 2);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
}

static void test_take_pending_multi_tap_hold_release_returns_held_lifecycle(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  release;
    keypos_t                   pos = test_keypos(6, 1);

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode         = TEST_MULTI_TAP_KEY,
        .owner.key_pos         = pos,
        .timing.tap_hold_term  = 120,
        .timing.longer_hold_term = 240,
        .pending_multi_tap =
            {
                .keycode      = TEST_MULTI_TAP_KEY,
                .key_pos      = pos,
                .count        = 2,
                .pending_hold = true,
                .tap_hold_term = 120,
                .hold         = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HOLD_ACTION),
                .long_hold    = hold_behavior_none(),
            },
    };

    release = test_step_handled_release(
        slot,
        TEST_MULTI_TAP_KEY,
        pos,
        (key_behavior_view_t){.keycode = TEST_MULTI_TAP_KEY});

    CHECK(release.handled);
    CHECK(release.count == 2);
    test_expect_held_register(&release, 0, pos, TEST_HOLD_ACTION);
    test_expect_held_unregister(&release, 1, pos, TEST_HOLD_ACTION);
    CHECK(key_runtime_slot_idle(slot));
}

static void test_take_pending_multi_tap_hold_release_prefers_release_long_hold_action(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  release;
    keypos_t                   pos = test_keypos(6, 5);

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode          = TEST_MULTI_TAP_KEY,
        .owner.key_pos          = pos,
        .timing.tap_hold_term   = 120,
        .timing.longer_hold_term = 240,
        .pending_multi_tap =
            {
                .keycode       = TEST_MULTI_TAP_KEY,
                .key_pos       = pos,
                .timer         = (uint16_t)(fake_time - 260),
                .count         = 2,
                .pending_hold  = true,
                .tap_hold_term = 120,
                .hold          = TAP_ON_RELEASE_AFTER_HOLD(TEST_HOLD_ACTION),
                .long_hold     = TAP_ON_RELEASE_AFTER_HOLD(TEST_SINGLE_ACTION),
            },
    };

    release = test_step_handled_release(
        slot,
        TEST_MULTI_TAP_KEY,
        pos,
        (key_behavior_view_t){.keycode = TEST_MULTI_TAP_KEY});

    CHECK(release.handled);
    CHECK(release.count == 1);
    CHECK(release.effects[0].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_DELAYED_ACTION);
    CHECK(release.effects[0].data.delayed_action.action == TEST_SINGLE_ACTION);
    CHECK(release.effects[0].data.delayed_action.repeat_count == 1);
    CHECK(key_runtime_slot_idle(slot));
}

static void test_take_pending_multi_tap_hold_release_preserves_chain_for_higher_taps(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  release;
    keypos_t                   pos = test_keypos(6, 2);

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode          = TEST_MULTI_TAP_KEY,
        .owner.key_pos          = pos,
        .timing.tap_hold_term   = 120,
        .timing.longer_hold_term = 240,
        .pending_multi_tap =
            {
                .keycode       = TEST_MULTI_TAP_KEY,
                .key_pos       = pos,
                .timer         = (uint16_t)(fake_time - 50),
                .count         = 2,
                .pending_hold  = true,
                .tap_action    = TEST_SINGLE_ACTION,
                .tap_hold_term = 120,
            },
    };

    release = test_step_handled_release(
        slot,
        TEST_MULTI_TAP_KEY,
        pos,
        (key_behavior_view_t){.keycode = TEST_MULTI_TAP_KEY});

    CHECK(release.handled);
    CHECK(release.count == 0);
    CHECK(slot->owner.keycode == KC_NO);
    CHECK(key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(slot->pending_multi_tap.keycode == TEST_MULTI_TAP_KEY);
    CHECK(slot->pending_multi_tap.key_pos.row == pos.row);
    CHECK(slot->pending_multi_tap.key_pos.col == pos.col);
    CHECK(slot->pending_multi_tap.count == 2);
    CHECK(!slot->pending_multi_tap.pending_hold);
}

static void test_take_pending_multi_tap_flush_prefers_exact_step_tap(void) {
    active_key_state_t                       *slot = key_runtime_primary_slot();
    key_runtime_slot_pending_multi_tap_flush_t flush;
    keypos_t                                  pos = test_keypos(6, 2);

    test_reset_state();

    slot->pending_multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .key_pos       = pos,
        .count         = 2,
        .single_action = TEST_SINGLE_ACTION,
    };

    flush = key_runtime_slot_take_pending_multi_tap_flush(slot);

    CHECK(flush.handled);
    CHECK(flush.action == TEST_SINGLE_ACTION);
    CHECK(flush.repeat_count == 2);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
}

static void test_prepare_handled_press_matching_pending_multi_tap_reuses_slot(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  plan;
    keypos_t                   pos = test_keypos(6, 6);

    test_reset_state();

    slot->pending_multi_tap = (multi_tap_t){
        .keycode         = TEST_MULTI_TAP_KEY,
        .key_pos         = pos,
        .count           = 1,
        .single_action   = TEST_SINGLE_ACTION,
        .tap_hold_term   = 120,
        .multi_tap_term  = 150,
    };

    plan = test_step_handled_press(
        slot,
        TEST_MULTI_TAP_KEY,
        pos,
        (handled_key_view_t){
            .behavior =
                {
                    .keycode            = TEST_MULTI_TAP_KEY,
                    .handled            = true,
                    .has_multi_tap      = true,
                    .is_momentary_layer = true,
                    .tap_hold_term      = 120,
                    .longer_hold_term   = 240,
                    .multi_tap_term     = 150,
                    .single =
                        {
                            .tap = TAP_SENDS(TEST_SINGLE_ACTION),
                        },
                },
        },
        false);

    CHECK(plan.handled);
    CHECK(plan.count == 1);
    test_expect_layer_press(&plan, 0, pos, 3);
    CHECK(slot->owner.keycode == TEST_MULTI_TAP_KEY);
    CHECK(slot->owner.key_pos.row == pos.row);
    CHECK(slot->owner.key_pos.col == pos.col);
    CHECK(slot->binding.tap_action == KC_NO);
    CHECK(slot->semantic.valid);
    CHECK(slot->semantic.has_multi_tap);
    CHECK(slot->semantic.is_momentary_layer);
    CHECK(slot->semantic.layer == 3);
    CHECK(key_runtime_slot_hold_is_complete(slot));
    CHECK(slot->pending_multi_tap.count == 2);
    CHECK(!slot->pending_multi_tap.pending_hold);
}

static void test_prepare_handled_press_flushes_pending_multi_tap_before_begin(void) {
    active_key_state_t        *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t  plan;
    keypos_t                   pending_pos = test_keypos(7, 0);
    keypos_t                   press_pos   = test_keypos(7, 1);

    test_reset_state();

    slot->pending_multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .key_pos       = pending_pos,
        .count         = 1,
        .single_action = TEST_SINGLE_ACTION,
    };

    test_pd_mode         = PD_MODE_VOLUME;
    test_pd_locked_modes = PD_MODE_VOLUME;

    plan = test_step_handled_press(
        slot,
        TEST_PD_MODE_KEY,
        press_pos,
        (handled_key_view_t){
            .behavior =
                {
                    .keycode          = TEST_PD_MODE_KEY,
                    .handled          = true,
                    .tap_hold_term    = 120,
                    .longer_hold_term = 240,
                    .multi_tap_term   = 150,
                    .single =
                        {
                            .tap  = TAP_SENDS(TEST_SINGLE_ACTION),
                            .hold =
                                {
                                    .present = true,
                                    .action  = TEST_HOLD_ACTION,
                                    .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                                },
                        },
                },
        },
        false);

    CHECK(plan.handled);
    CHECK(plan.count == 2);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_DELAYED_ACTION);
    CHECK(plan.effects[0].data.delayed_action.action == TEST_SINGLE_ACTION);
    CHECK(plan.effects[0].data.delayed_action.repeat_count == 1);
    test_expect_held_register(&plan, 1, press_pos, TEST_HOLD_ACTION);
    CHECK(slot->owner.keycode == TEST_PD_MODE_KEY);
    CHECK(slot->owner.key_pos.row == press_pos.row);
    CHECK(slot->owner.key_pos.col == press_pos.col);
    CHECK(slot->binding.tap_action == TEST_SINGLE_ACTION);
    CHECK(slot->lifecycle.held_action_keycode == TEST_HOLD_ACTION);
    CHECK(key_runtime_slot_uses_implicit_hold(slot));
    CHECK(slot->lifecycle.phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW);
    CHECK(slot->lifecycle.pd_mode_was_locked_on_press);
    CHECK(slot->semantic.valid);
    CHECK(slot->semantic.pd_mode == PD_MODE_VOLUME);
    CHECK(slot->semantic.preview_layer == UINT8_MAX);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
}

static void test_take_handled_release_returns_cleanup_for_unmatched_release(void) {
    key_runtime_slot_result_t release;
    keypos_t                  pos = test_keypos(7, 3);

    test_reset_state();
    key_runtime_primary_slot()->owner.keycode = TEST_ACTIVE_KEY;
    key_runtime_primary_slot()->owner.key_pos = test_keypos(7, 4);

    release = test_step_handled_release(
        key_runtime_primary_slot(),
        TEST_ACTIVE_KEY,
        pos,
        (key_behavior_view_t){
            .keycode            = TEST_ACTIVE_KEY,
            .is_momentary_layer = true,
        });

    CHECK(release.handled);
    CHECK(release.count == 2);
    test_expect_layer_release(&release, 0, pos);
    test_expect_release_owned_state(&release, 1, pos);
}

static void test_take_handled_press_result_maps_flush_and_begin_request(void) {
    active_key_state_t         *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t   result;
    keypos_t                    pending_pos = test_keypos(7, 0);
    keypos_t                    press_pos   = test_keypos(7, 1);

    test_reset_state();

    slot->pending_multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .key_pos       = pending_pos,
        .count         = 1,
        .single_action = TEST_SINGLE_ACTION,
    };

    test_pd_mode         = PD_MODE_VOLUME;
    test_pd_locked_modes = PD_MODE_VOLUME;

    result = test_step_handled_press(
        slot,
        TEST_PD_MODE_KEY,
        press_pos,
        (handled_key_view_t){
            .behavior =
                {
                    .keycode          = TEST_PD_MODE_KEY,
                    .handled          = true,
                    .tap_hold_term    = 120,
                    .longer_hold_term = 240,
                    .multi_tap_term   = 150,
                    .single =
                        {
                            .tap  = TAP_SENDS(TEST_SINGLE_ACTION),
                            .hold =
                                {
                                    .present = true,
                                    .action  = TEST_HOLD_ACTION,
                                    .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                                },
                        },
                },
        },
        false);

    CHECK(result.handled);
    CHECK(result.count == 2);
    CHECK(result.effects[0].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_DELAYED_ACTION);
    CHECK(result.effects[0].data.delayed_action.action == TEST_SINGLE_ACTION);
    CHECK(result.effects[0].data.delayed_action.repeat_count == 1);
    test_expect_held_register(&result, 1, press_pos, TEST_HOLD_ACTION);
}

static void test_take_handled_press_result_reclaims_held_action_before_begin_request(void) {
    active_key_state_t         *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t   result;
    keypos_t                    previous_pos = test_keypos(7, 2);
    keypos_t                    press_pos    = test_keypos(7, 3);

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode                 = TEST_ACTIVE_KEY,
        .owner.key_pos                 = previous_pos,
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
        .lifecycle.held_action_keycode = TEST_SINGLE_ACTION,
    };

    result = test_step_handled_press(
        slot,
        TEST_PD_MODE_KEY,
        press_pos,
        (handled_key_view_t){
            .behavior =
                {
                    .keycode          = TEST_PD_MODE_KEY,
                    .handled          = true,
                    .tap_hold_term    = 120,
                    .longer_hold_term = 240,
                    .multi_tap_term   = 150,
                    .single =
                        {
                            .hold =
                                {
                                    .present = true,
                                    .action  = TEST_HOLD_ACTION,
                                    .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                                },
                        },
                },
        },
        false);

    CHECK(result.handled);
    CHECK(result.count == 2);
    test_expect_held_unregister(&result, 0, previous_pos, TEST_SINGLE_ACTION);
    test_expect_held_register(&result, 1, press_pos, TEST_HOLD_ACTION);
}

static void test_take_handled_press_result_dispatches_tap_flush_before_begin_request(void) {
    active_key_state_t         *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t   result;
    keypos_t                    previous_pos = test_keypos(7, 4);
    keypos_t                    press_pos    = test_keypos(7, 5);

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode      = TEST_ACTIVE_KEY,
        .owner.key_pos      = previous_pos,
        .lifecycle.phase    = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .binding.tap_action = TEST_SINGLE_ACTION,
    };

    result = test_step_handled_press(
        slot,
        TEST_PD_MODE_KEY,
        press_pos,
        (handled_key_view_t){
            .behavior =
                {
                    .keycode          = TEST_PD_MODE_KEY,
                    .handled          = true,
                    .tap_hold_term    = 120,
                    .longer_hold_term = 240,
                    .multi_tap_term   = 150,
                    .single =
                        {
                            .hold =
                                {
                                    .present = true,
                                    .action  = TEST_HOLD_ACTION,
                                    .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                                },
                        },
                },
        },
        false);

    CHECK(result.handled);
    CHECK(result.count == 2);
    test_expect_dispatch_action(&result, 0, TEST_SINGLE_ACTION);
    test_expect_held_register(&result, 1, press_pos, TEST_HOLD_ACTION);
}

static void test_take_handled_release_result_maps_pending_multi_tap_held_lifecycle(void) {
    active_key_state_t         *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t   result;
    keypos_t                    pos = test_keypos(6, 1);

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode          = TEST_MULTI_TAP_KEY,
        .owner.key_pos          = pos,
        .timing.tap_hold_term   = 120,
        .timing.longer_hold_term = 240,
        .pending_multi_tap =
            {
                .keycode       = TEST_MULTI_TAP_KEY,
                .key_pos       = pos,
                .count         = 2,
                .pending_hold  = true,
                .tap_hold_term = 120,
                .hold          = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_HOLD_ACTION),
                .long_hold     = hold_behavior_none(),
            },
    };

    result = test_step_handled_release(
        slot,
        TEST_MULTI_TAP_KEY,
        pos,
        (key_behavior_view_t){.keycode = TEST_MULTI_TAP_KEY});

    CHECK(result.handled);
    CHECK(result.count == 2);
    test_expect_held_register(&result, 0, pos, TEST_HOLD_ACTION);
    test_expect_held_unregister(&result, 1, pos, TEST_HOLD_ACTION);
}

static void test_take_active_scan_result_maps_commit_and_long_hold_requests(void) {
    active_key_state_t         *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t   result;

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode                 = TEST_ACTIVE_KEY,
        .owner.key_pos                 = test_keypos(3, 1),
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
        .timer                         = (uint16_t)(fake_time - 260),
        .timing.tap_hold_term          = 120,
        .timing.longer_hold_term       = 240,
        .lifecycle.held_action_keycode = TEST_HOLD_ACTION,
        .binding.hold                  =
            {
                .present = true,
                .action  = TEST_HOLD_ACTION,
                .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
            },
        .binding.long_hold             = TAP_AT_HOLD_THRESHOLD(TEST_SINGLE_ACTION),
    };

    result = test_step_active_scan(slot);

    CHECK(result.handled);
    CHECK(result.count == 4);
    test_expect_feedback_pulse(&result, 0, false);
    test_expect_release_owned_state(&result, 1, slot->owner.key_pos);
    test_expect_dispatch_action(&result, 2, TEST_SINGLE_ACTION);
    test_expect_feedback_pulse(&result, 3, true);
}

static void test_take_active_scan_result_starts_repeat_binding(void) {
    active_key_state_t         *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t   result;

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode        = TEST_ACTIVE_KEY,
        .owner.key_pos        = test_keypos(3, 3),
        .lifecycle.phase      = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .timer                = (uint16_t)(fake_time - 130),
        .timing.tap_hold_term = 120,
        .binding.hold         = REPEAT_WHILE_HELD(TEST_HOLD_ACTION, 25),
    };

    result = test_step_active_scan(slot);

    CHECK(result.handled);
    CHECK(result.count == 2);
    test_expect_repeat_start(&result, 0, slot->owner.key_pos, TEST_HOLD_ACTION, 25);
    test_expect_feedback_pulse(&result, 1, false);
    CHECK(slot->lifecycle.repeat_binding_active);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_interrupt_result_maps_slot_effect_request(void) {
    active_key_state_t         *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t   result;

    test_reset_state();

    *slot = (active_key_state_t){
        .owner.keycode           = TEST_LAYER_KEY,
        .owner.key_pos           = test_keypos(6, 2),
        .lifecycle.phase         = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .lifecycle.hold_strategy = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK,
    };

    result = test_step_interrupt(slot, test_keypos(6, 3));

    CHECK(result.handled);
    CHECK(result.count == 1);
    test_expect_held_register(&result, 0, slot->owner.key_pos, TEST_LAYER_KEY);
}

int main(void) {
    test_slot_pending_multi_tap_ownership_marks_slot_non_idle();
    test_slot_pending_multi_tap_lifecycle_helpers();
    test_slot_track_preserves_pending_multi_tap();
    test_resolve_pending_multi_tap_hold_clears_slot_owned_state();
    test_take_active_release_starts_pending_multi_tap_chain();
    test_take_active_release_quick_immediate_hold_becomes_tap();
    test_take_active_release_maps_locked_pd_mode_tap();
    test_take_active_release_release_hold_pending_dispatches_release_hold_action();
    test_take_active_release_hold_tier_active_dispatches_release_long_hold();
    test_take_active_scan_event_promotes_long_hold_after_longer_term();
    test_take_active_scan_event_returns_commit_and_long_hold_requests();
    test_take_active_scan_event_release_hold_pending_promotes_long_hold();
    test_take_active_scan_event_hold_tier_active_promotes_long_hold_and_releases_owned_state();
    test_take_active_scan_event_returns_fallback_hold_request();
    test_take_pending_multi_tap_scan_event_returns_long_hold_request();
    test_take_pending_multi_tap_scan_event_returns_threshold_hold_request();
    test_take_pending_multi_tap_scan_event_flushes_expired_chain();
    test_take_pending_multi_tap_hold_release_returns_held_lifecycle();
    test_take_pending_multi_tap_hold_release_prefers_release_long_hold_action();
    test_take_pending_multi_tap_hold_release_preserves_chain_for_higher_taps();
    test_take_pending_multi_tap_flush_prefers_exact_step_tap();
    test_prepare_handled_press_matching_pending_multi_tap_reuses_slot();
    test_prepare_handled_press_flushes_pending_multi_tap_before_begin();
    test_take_handled_press_result_reclaims_held_action_before_begin_request();
    test_take_handled_press_result_dispatches_tap_flush_before_begin_request();
    test_take_handled_release_returns_cleanup_for_unmatched_release();
    test_take_handled_press_result_maps_flush_and_begin_request();
    test_take_handled_release_result_maps_pending_multi_tap_held_lifecycle();
    test_take_active_scan_result_maps_commit_and_long_hold_requests();
    test_take_active_scan_result_starts_repeat_binding();
    test_take_interrupt_result_maps_slot_effect_request();

    puts("key_runtime_slot host tests passed");
    return 0;
}
