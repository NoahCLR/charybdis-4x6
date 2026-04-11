#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_lifecycle.h"
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

bool is_layer_key(uint16_t keycode) {
    return keycode == TEST_LAYER_KEY;
}

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    (void)action;
    return NOAH_ACTION_HOLD_KIND_SHARED;
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    (void)mt;
    return (delayed_action_mods_t){0};
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
    CHECK(key_runtime_find_slot_with_pending_multi_tap(pos) == slot);
    CHECK(key_runtime_find_reclaimable_slot() == slot);
}

static void test_select_slot_for_press_prefers_slot_owning_pending_multi_tap(void) {
    active_key_state_t *primary   = key_runtime_primary_slot();
    active_key_state_t *secondary = key_runtime_slot_at(1);
    keypos_t            target    = test_keypos(6, 2);

    test_reset_state();
    primary->keycode = TEST_ACTIVE_KEY;
    primary->key_pos = test_keypos(1, 1);
    secondary->pending_multi_tap = (multi_tap_t){
        .keycode = TEST_MULTI_TAP_KEY,
        .key_pos = target,
        .count   = 1,
    };

    CHECK(key_runtime_select_slot_for_press(target) == secondary);
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

    key_runtime_slot_track(slot, TEST_MULTI_TAP_KEY, pos, KC_NO, hold_behavior_none(), hold_behavior_none(), 120, 240, 150, false);

    CHECK(slot->keycode == TEST_MULTI_TAP_KEY);
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

static void test_resolve_release_quick_immediate_hold_becomes_tap(void) {
    key_runtime_slot_release_resolution_t resolution;
    keypos_t                             pos = test_keypos(2, 1);

    test_reset_state();

    resolution = key_runtime_slot_resolve_release(
        TEST_ACTIVE_KEY,
        (active_key_state_t){
            .keycode             = TEST_ACTIVE_KEY,
            .key_pos             = pos,
            .held_action_keycode = TEST_HOLD_ACTION,
            .tap_action          = TEST_SINGLE_ACTION,
            .tap_hold_term       = 150,
            .hold =
                {
                    .present = true,
                    .action  = TEST_HOLD_ACTION,
                    .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                },
        },
        (key_behavior_view_t){
            .keycode       = TEST_ACTIVE_KEY,
            .tap_hold_term = 150,
        },
        50);

    CHECK(resolution.release_owned_state);
    CHECK(resolution.outcome == KEY_RUNTIME_SLOT_RELEASE_OUTCOME_TAP);
}

static void test_resolve_release_locked_pd_mode_becomes_lock_tap(void) {
    key_runtime_slot_release_resolution_t resolution;
    keypos_t                             pos = test_keypos(2, 2);

    test_reset_state();
    test_pd_mode         = PD_MODE_VOLUME;
    test_pd_locked_modes = PD_MODE_VOLUME;

    resolution = key_runtime_slot_resolve_release(
        TEST_PD_MODE_KEY,
        (active_key_state_t){
            .keycode                     = TEST_PD_MODE_KEY,
            .key_pos                     = pos,
            .tap_hold_term               = 150,
            .pd_mode_was_locked_on_press = true,
        },
        (key_behavior_view_t){
            .keycode       = TEST_PD_MODE_KEY,
            .tap_hold_term = 150,
        },
        50);

    CHECK(!resolution.release_owned_state);
    CHECK(resolution.outcome == KEY_RUNTIME_SLOT_RELEASE_OUTCOME_PD_MODE_LOCK_TAP);
    CHECK(resolution.pd_mode_lock_tap == PD_MODE_VOLUME);
}

static void test_resolve_scan_promotes_long_hold_after_longer_term(void) {
    key_runtime_slot_scan_resolution_t resolution;

    test_reset_state();

    resolution = key_runtime_slot_resolve_scan(
        (active_key_state_t){
            .keycode          = TEST_ACTIVE_KEY,
            .tap_hold_term    = 120,
            .longer_hold_term = 240,
            .hold             = TAP_AT_HOLD_THRESHOLD(TEST_SINGLE_ACTION),
            .long_hold        = TAP_AT_HOLD_THRESHOLD(TEST_HOLD_ACTION),
        },
        260);

    CHECK(!resolution.activate_fallback_hold);
    CHECK(resolution.outcome == KEY_RUNTIME_SLOT_SCAN_OUTCOME_PROMOTE_LONG_HOLD);
    CHECK(resolution.long_hold.action == TEST_HOLD_ACTION);
}

static void test_resolve_pending_multi_tap_scan_requires_layer_release_before_lock(void) {
    key_runtime_slot_pending_multi_tap_scan_resolution_t resolution;

    test_reset_state();

    resolution = key_runtime_slot_resolve_pending_multi_tap_scan(
        (active_key_state_t){
            .keycode          = TEST_LAYER_KEY,
            .longer_hold_term = 240,
        },
        (multi_tap_t){
            .pending_hold = true,
            .long_hold    = TAP_AT_HOLD_THRESHOLD(TEST_LAYER_LOCK),
        },
        260);

    CHECK(resolution.outcome == KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD);
    CHECK(resolution.release_layer_before_action);
    CHECK(resolution.long_hold.action == TEST_LAYER_LOCK);
}

static void test_fire_hold_at_threshold_starts_repeat_binding(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode  = TEST_ACTIVE_KEY,
        .key_pos  = test_keypos(3, 3),
        .hold     = REPEAT_WHILE_HELD(TEST_HOLD_ACTION, 25),
    };

    request = key_runtime_slot_fire_hold_at_threshold(slot, slot->hold, hold_behavior_none(), false);

    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_REPEAT_START);
    CHECK(request.action == TEST_HOLD_ACTION);
    CHECK(request.repeat_hz == 25);
    CHECK(request.feedback_pulse);
    CHECK(!request.feedback_long_hold_level);
    CHECK(!request.release_owned_state);
    CHECK(slot->repeat_binding_active);
    CHECK(slot->hold_fired);
    CHECK(!slot->hold_one_shot_fired);
}

static void test_promote_to_long_hold_releases_owned_state_before_dispatch(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode             = TEST_ACTIVE_KEY,
        .key_pos             = test_keypos(3, 4),
        .held_action_keycode = TEST_SINGLE_ACTION,
    };

    request = key_runtime_slot_promote_to_long_hold(slot, (hold_behavior_t)TAP_AT_HOLD_THRESHOLD(TEST_HOLD_ACTION), false);

    CHECK(request.release_owned_state);
    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION);
    CHECK(request.action == TEST_HOLD_ACTION);
    CHECK(request.feedback_pulse);
    CHECK(request.feedback_long_hold_level);
    CHECK(slot->held_action_keycode == KC_NO);
    CHECK(slot->hold_fired);
}

static void test_apply_pending_multi_tap_scan_resolution_consumes_pending_chain(void) {
    active_key_state_t                          *slot = key_runtime_primary_slot();
    key_runtime_slot_pending_multi_tap_scan_apply_t apply;
    keypos_t                                     pos = test_keypos(6, 0);

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode = TEST_LAYER_KEY,
        .key_pos = pos,
        .pending_multi_tap =
            {
                .keycode      = TEST_MULTI_TAP_KEY,
                .key_pos      = pos,
                .count        = 2,
                .pending_hold = true,
            },
    };

    apply = key_runtime_slot_apply_pending_multi_tap_scan_resolution(
        slot,
        (key_runtime_slot_pending_multi_tap_scan_resolution_t){
            .outcome                     = KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_SCAN_OUTCOME_PROMOTE_LONG_HOLD,
            .release_layer_before_action = true,
            .long_hold                   = TAP_AT_HOLD_THRESHOLD(TEST_LAYER_LOCK),
        });

    CHECK(apply.release_layer_before_action);
    CHECK(apply.effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION);
    CHECK(apply.effect_request.action == TEST_LAYER_LOCK);
    CHECK(apply.effect_request.feedback_pulse);
    CHECK(apply.effect_request.feedback_long_hold_level);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(slot->long_hold.action == TEST_LAYER_LOCK);
    CHECK(slot->hold_fired);
}

static void test_take_pending_multi_tap_hold_release_returns_held_lifecycle(void) {
    active_key_state_t                               *slot = key_runtime_primary_slot();
    key_runtime_slot_pending_multi_tap_hold_release_t release;
    keypos_t                                          pos = test_keypos(6, 1);

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode          = TEST_MULTI_TAP_KEY,
        .key_pos          = pos,
        .tap_hold_term    = 120,
        .longer_hold_term = 240,
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

    release = key_runtime_slot_take_pending_multi_tap_hold_release(
        slot,
        TEST_MULTI_TAP_KEY,
        (key_behavior_view_t){.keycode = TEST_MULTI_TAP_KEY},
        150);

    CHECK(release.handled);
    CHECK(!release.release_layer_after_action);
    CHECK(release.key_pos.row == pos.row);
    CHECK(release.key_pos.col == pos.col);
    CHECK(release.outcome == KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_HELD_LIFECYCLE);
    CHECK(release.action == TEST_HOLD_ACTION);
    CHECK(release.repeat_count == 1);
    CHECK(key_runtime_slot_idle(slot));
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

static void test_begin_press_sets_metadata_and_immediate_hold_request(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;
    keypos_t                          pos = test_keypos(6, 3);

    test_reset_state();

    request = key_runtime_slot_begin_press(
        slot,
        TEST_ACTIVE_KEY,
        pos,
        TEST_SINGLE_ACTION,
        (hold_behavior_t){
            .present = true,
            .action  = TEST_HOLD_ACTION,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        },
        hold_behavior_none(),
        120,
        240,
        150,
        false,
        true,
        false,
        true);

    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER);
    CHECK(request.action == TEST_HOLD_ACTION);
    CHECK(slot->keycode == TEST_ACTIVE_KEY);
    CHECK(slot->tap_action == TEST_SINGLE_ACTION);
    CHECK(slot->held_action_keycode == TEST_HOLD_ACTION);
    CHECK(slot->implicit_hold);
    CHECK(!slot->fallback_hold_pending);
    CHECK(slot->pd_mode_was_locked_on_press);
}

static void test_prepare_handled_press_matching_pending_multi_tap_reuses_slot(void) {
    active_key_state_t             *slot = key_runtime_primary_slot();
    key_runtime_slot_press_plan_t   plan;
    keypos_t                        pos = test_keypos(6, 6);

    test_reset_state();

    slot->pending_multi_tap = (multi_tap_t){
        .keycode         = TEST_MULTI_TAP_KEY,
        .key_pos         = pos,
        .count           = 1,
        .single_action   = TEST_SINGLE_ACTION,
        .tap_hold_term   = 120,
        .multi_tap_term  = 150,
    };

    plan = key_runtime_slot_prepare_handled_press(
        slot,
        TEST_MULTI_TAP_KEY,
        pos,
        false,
        true,
        true,
        3,
        TEST_SINGLE_ACTION,
        hold_behavior_none(),
        hold_behavior_none(),
        120,
        240,
        150,
        false,
        false,
        false);

    CHECK(plan.handled);
    CHECK(!plan.pending_multi_tap_flush.handled);
    CHECK(plan.dispatch_action == KC_NO);
    CHECK(plan.layer_press);
    CHECK(plan.layer == 3);
    CHECK(plan.reclaim_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE);
    CHECK(plan.begin_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE);
    CHECK(slot->keycode == TEST_MULTI_TAP_KEY);
    CHECK(slot->key_pos.row == pos.row);
    CHECK(slot->key_pos.col == pos.col);
    CHECK(slot->tap_action == KC_NO);
    CHECK(slot->hold_fired);
    CHECK(slot->pending_multi_tap.count == 2);
    CHECK(!slot->pending_multi_tap.pending_hold);
}

static void test_prepare_handled_press_flushes_pending_multi_tap_before_begin(void) {
    active_key_state_t             *slot = key_runtime_primary_slot();
    key_runtime_slot_press_plan_t   plan;
    keypos_t                        pending_pos = test_keypos(7, 0);
    keypos_t                        press_pos   = test_keypos(7, 1);

    test_reset_state();

    slot->pending_multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .key_pos       = pending_pos,
        .count         = 1,
        .single_action = TEST_SINGLE_ACTION,
    };

    plan = key_runtime_slot_prepare_handled_press(
        slot,
        TEST_ACTIVE_KEY,
        press_pos,
        false,
        false,
        false,
        0,
        TEST_SINGLE_ACTION,
        (hold_behavior_t){
            .present = true,
            .action  = TEST_HOLD_ACTION,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        },
        hold_behavior_none(),
        120,
        240,
        150,
        true,
        false,
        true);

    CHECK(plan.handled);
    CHECK(plan.pending_multi_tap_flush.handled);
    CHECK(plan.pending_multi_tap_flush.action == TEST_SINGLE_ACTION);
    CHECK(plan.pending_multi_tap_flush.repeat_count == 1);
    CHECK(plan.reclaim_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE);
    CHECK(plan.begin_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER);
    CHECK(plan.begin_request.action == TEST_HOLD_ACTION);
    CHECK(slot->keycode == TEST_ACTIVE_KEY);
    CHECK(slot->key_pos.row == press_pos.row);
    CHECK(slot->key_pos.col == press_pos.col);
    CHECK(slot->tap_action == TEST_SINGLE_ACTION);
    CHECK(slot->held_action_keycode == TEST_HOLD_ACTION);
    CHECK(slot->implicit_hold);
    CHECK(slot->pd_mode_was_locked_on_press);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
}

static void test_interrupt_on_other_press_marks_layer_and_activates_fallback_hold(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode               = TEST_LAYER_KEY,
        .key_pos               = test_keypos(6, 2),
        .fallback_hold_pending = true,
    };

    request = key_runtime_slot_interrupt_on_other_press(slot, test_keypos(6, 3));

    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER);
    CHECK(request.action == TEST_LAYER_KEY);
    CHECK(slot->held_action_keycode == TEST_LAYER_KEY);
    CHECK(slot->hold_fired);
    CHECK(slot->layer_interrupted);
}

static void test_commit_immediate_hold_sets_flags_and_feedback_request(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode = TEST_ACTIVE_KEY,
        .key_pos = test_keypos(6, 3),
    };

    request = key_runtime_slot_commit_immediate_hold(slot, true, true);

    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE);
    CHECK(request.feedback_pulse);
    CHECK(!request.feedback_long_hold_level);
    CHECK(slot->hold_one_shot_fired);
    CHECK(slot->hold_fired);
}

static void test_take_flush_unregisters_held_action(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode             = TEST_ACTIVE_KEY,
        .key_pos             = test_keypos(6, 4),
        .held_action_keycode = TEST_HOLD_ACTION,
        .hold_fired          = true,
    };

    request = key_runtime_slot_take_flush(slot, false);

    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER);
    CHECK(request.action == TEST_HOLD_ACTION);
    CHECK(key_runtime_slot_idle(slot));
}

static void test_take_flush_dispatches_tap_for_unheld_non_layer_key(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode    = TEST_ACTIVE_KEY,
        .key_pos    = test_keypos(6, 5),
        .tap_action = TEST_SINGLE_ACTION,
    };

    request = key_runtime_slot_take_flush(slot, false);

    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION);
    CHECK(request.action == TEST_SINGLE_ACTION);
    CHECK(key_runtime_slot_idle(slot));
}

int main(void) {
    test_slot_pending_multi_tap_ownership_marks_slot_non_idle();
    test_select_slot_for_press_prefers_slot_owning_pending_multi_tap();
    test_slot_pending_multi_tap_lifecycle_helpers();
    test_slot_track_preserves_pending_multi_tap();
    test_resolve_pending_multi_tap_hold_clears_slot_owned_state();
    test_resolve_release_quick_immediate_hold_becomes_tap();
    test_resolve_release_locked_pd_mode_becomes_lock_tap();
    test_resolve_scan_promotes_long_hold_after_longer_term();
    test_resolve_pending_multi_tap_scan_requires_layer_release_before_lock();
    test_fire_hold_at_threshold_starts_repeat_binding();
    test_promote_to_long_hold_releases_owned_state_before_dispatch();
    test_apply_pending_multi_tap_scan_resolution_consumes_pending_chain();
    test_take_pending_multi_tap_hold_release_returns_held_lifecycle();
    test_take_pending_multi_tap_flush_prefers_exact_step_tap();
    test_begin_press_sets_metadata_and_immediate_hold_request();
    test_prepare_handled_press_matching_pending_multi_tap_reuses_slot();
    test_prepare_handled_press_flushes_pending_multi_tap_before_begin();
    test_interrupt_on_other_press_marks_layer_and_activates_fallback_hold();
    test_commit_immediate_hold_sets_flags_and_feedback_request();
    test_take_flush_unregisters_held_action();
    test_take_flush_dispatches_tap_for_unheld_non_layer_key();

    puts("key_runtime_slot host tests passed");
    return 0;
}
