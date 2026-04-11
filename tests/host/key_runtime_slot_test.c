#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/key_runtime_slot_effect.h"
#include "users/noah/lib/key/key_runtime_slot_press.h"
#include "users/noah/lib/key/key_runtime_slot_release.h"
#include "users/noah/lib/key/key_runtime_slot_result.h"
#include "users/noah/lib/key/key_runtime_slot_scan.h"
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

uint16_t handled_key_tap_action(handled_key_view_t key) {
    if (key.behavior.single.tap.present) return key.behavior.single.tap.action;
    if (pd_mode_for_keycode(key.behavior.keycode)) return KC_NO;
    if (test_handled_key_uses_buffered_modifier_single_step(key)) return KC_NO;
    if (key.behavior.is_layer_tap) return QK_LAYER_TAP_GET_TAP_KEYCODE(key.behavior.keycode);
    if (key.behavior.is_momentary_layer) return KC_NO;
    if (key.behavior.keycode >= SAFE_RANGE) return KC_NO;
    return key.behavior.keycode;
}

void held_action_register(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
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
            .phase               = KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
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

static void test_take_active_release_starts_pending_multi_tap_chain(void) {
    active_key_state_t                *slot = key_runtime_primary_slot();
    key_runtime_slot_release_apply_t   apply;
    keypos_t                           pos = test_keypos(2, 3);

    test_reset_state();

    *slot = (active_key_state_t){
        .timer          = (uint16_t)(fake_time - 50),
        .keycode        = TEST_MULTI_TAP_KEY,
        .key_pos        = pos,
        .tap_action     = TEST_SINGLE_ACTION,
        .tap_hold_term  = 120,
        .multi_tap_term = 150,
    };

    apply = key_runtime_slot_take_active_release(
        slot,
        TEST_MULTI_TAP_KEY,
        (key_behavior_view_t){
            .keycode        = TEST_MULTI_TAP_KEY,
            .has_multi_tap  = true,
            .tap_hold_term  = 120,
        });

    CHECK(apply.handled);
    CHECK(!apply.release_layer);
    CHECK(!apply.release_owned_state);
    CHECK(apply.outcome == KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_NONE);
    CHECK(slot->keycode == KC_NO);
    CHECK(key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(slot->pending_multi_tap.keycode == TEST_MULTI_TAP_KEY);
    CHECK(slot->pending_multi_tap.key_pos.row == pos.row);
    CHECK(slot->pending_multi_tap.key_pos.col == pos.col);
    CHECK(slot->pending_multi_tap.count == 1);
    CHECK(slot->pending_multi_tap.single_action == TEST_SINGLE_ACTION);
}

static void test_take_active_release_maps_locked_pd_mode_tap(void) {
    active_key_state_t                *slot = key_runtime_primary_slot();
    key_runtime_slot_release_apply_t   apply;
    keypos_t                           pos = test_keypos(2, 4);

    test_reset_state();
    test_pd_mode         = PD_MODE_VOLUME;
    test_pd_locked_modes = PD_MODE_VOLUME;

    *slot = (active_key_state_t){
        .timer                      = (uint16_t)(fake_time - 50),
        .keycode                    = TEST_PD_MODE_KEY,
        .key_pos                    = pos,
        .tap_hold_term              = 120,
        .pd_mode_was_locked_on_press = true,
    };

    apply = key_runtime_slot_take_active_release(
        slot,
        TEST_PD_MODE_KEY,
        (key_behavior_view_t){
            .keycode       = TEST_PD_MODE_KEY,
            .tap_hold_term = 120,
        });

    CHECK(apply.handled);
    CHECK(!apply.release_layer);
    CHECK(!apply.release_owned_state);
    CHECK(apply.outcome == KEY_RUNTIME_SLOT_RELEASE_APPLY_OUTCOME_PD_MODE_LOCK_TAP);
    CHECK(apply.pd_mode_lock_tap == PD_MODE_VOLUME);
    CHECK(key_runtime_slot_idle(slot));
}

static void test_take_active_scan_event_promotes_long_hold_after_longer_term(void) {
    active_key_state_t          *slot = key_runtime_primary_slot();
    key_runtime_slot_scan_event_t event;
    keypos_t                     pos = test_keypos(3, 1);

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode          = TEST_ACTIVE_KEY,
        .key_pos          = pos,
        .phase            = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .timer            = (uint16_t)(fake_time - 260),
        .tap_hold_term    = 120,
        .longer_hold_term = 240,
        .hold             = TAP_AT_HOLD_THRESHOLD(TEST_SINGLE_ACTION),
        .long_hold        = TAP_AT_HOLD_THRESHOLD(TEST_HOLD_ACTION),
    };

    event = key_runtime_slot_take_active_scan_event(slot);

    CHECK(event.handled);
    CHECK(event.kind == KEY_RUNTIME_SLOT_SCAN_EVENT_ACTIVE_EFFECTS);
    CHECK(event.key_pos.row == pos.row);
    CHECK(event.key_pos.col == pos.col);
    CHECK(event.data.active_effects.immediate_hold_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE);
    CHECK(event.data.active_effects.effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION);
    CHECK(event.data.active_effects.effect_request.action == TEST_HOLD_ACTION);
    CHECK(event.data.active_effects.effect_request.feedback_pulse);
    CHECK(event.data.active_effects.effect_request.feedback_long_hold_level);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_active_scan_event_returns_commit_and_long_hold_requests(void) {
    active_key_state_t          *slot = key_runtime_primary_slot();
    key_runtime_slot_scan_event_t event;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode             = TEST_ACTIVE_KEY,
        .key_pos             = test_keypos(3, 1),
        .phase               = KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
        .timer               = (uint16_t)(fake_time - 260),
        .tap_hold_term       = 120,
        .longer_hold_term    = 240,
        .held_action_keycode = TEST_HOLD_ACTION,
        .hold                =
            {
                .present = true,
                .action  = TEST_HOLD_ACTION,
                .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
            },
        .long_hold           = TAP_AT_HOLD_THRESHOLD(TEST_SINGLE_ACTION),
    };

    event = key_runtime_slot_take_active_scan_event(slot);

    CHECK(event.handled);
    CHECK(event.kind == KEY_RUNTIME_SLOT_SCAN_EVENT_ACTIVE_EFFECTS);
    CHECK(event.data.active_effects.immediate_hold_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE);
    CHECK(event.data.active_effects.immediate_hold_request.feedback_pulse);
    CHECK(!event.data.active_effects.immediate_hold_request.feedback_long_hold_level);
    CHECK(event.data.active_effects.effect_request.release_owned_state);
    CHECK(event.data.active_effects.effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION);
    CHECK(event.data.active_effects.effect_request.action == TEST_SINGLE_ACTION);
    CHECK(event.data.active_effects.effect_request.feedback_pulse);
    CHECK(event.data.active_effects.effect_request.feedback_long_hold_level);
    CHECK(key_runtime_slot_hold_is_complete(slot));
    CHECK(slot->held_action_keycode == KC_NO);
}

static void test_take_active_scan_event_returns_fallback_hold_request(void) {
    active_key_state_t          *slot = key_runtime_primary_slot();
    key_runtime_slot_scan_event_t event;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode        = TEST_ACTIVE_KEY,
        .key_pos        = test_keypos(3, 2),
        .phase          = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .timer          = (uint16_t)(fake_time - 120),
        .tap_hold_term  = 100,
        .hold_strategy  = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK,
    };

    event = key_runtime_slot_take_active_scan_event(slot);

    CHECK(event.handled);
    CHECK(event.kind == KEY_RUNTIME_SLOT_SCAN_EVENT_ACTIVE_EFFECTS);
    CHECK(event.data.active_effects.immediate_hold_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE);
    CHECK(event.data.active_effects.effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER);
    CHECK(event.data.active_effects.effect_request.action == TEST_ACTIVE_KEY);
    CHECK(slot->held_action_keycode == TEST_ACTIVE_KEY);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_fire_hold_at_threshold_starts_repeat_binding(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode  = TEST_ACTIVE_KEY,
        .key_pos  = test_keypos(3, 3),
        .phase    = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
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
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_promote_to_long_hold_releases_owned_state_before_dispatch(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode             = TEST_ACTIVE_KEY,
        .key_pos             = test_keypos(3, 4),
        .phase               = KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE,
        .held_action_keycode = TEST_SINGLE_ACTION,
    };

    request = key_runtime_slot_promote_to_long_hold(slot, (hold_behavior_t)TAP_AT_HOLD_THRESHOLD(TEST_HOLD_ACTION), false);

    CHECK(request.release_owned_state);
    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION);
    CHECK(request.action == TEST_HOLD_ACTION);
    CHECK(request.feedback_pulse);
    CHECK(request.feedback_long_hold_level);
    CHECK(slot->held_action_keycode == KC_NO);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_pending_multi_tap_scan_event_returns_long_hold_request(void) {
    active_key_state_t          *slot = key_runtime_primary_slot();
    key_runtime_slot_scan_event_t event;
    keypos_t                     pos = test_keypos(6, 0);

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode          = TEST_LAYER_KEY,
        .key_pos          = pos,
        .phase            = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .longer_hold_term = 240,
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

    event = key_runtime_slot_take_pending_multi_tap_scan_event(slot);

    CHECK(event.handled);
    CHECK(event.kind == KEY_RUNTIME_SLOT_SCAN_EVENT_PENDING_MULTI_TAP_EFFECTS);
    CHECK(event.key_pos.row == pos.row);
    CHECK(event.key_pos.col == pos.col);
    CHECK(event.data.pending_multi_tap_effects.release_layer_before_action);
    CHECK(event.data.pending_multi_tap_effects.effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION);
    CHECK(event.data.pending_multi_tap_effects.effect_request.action == TEST_LAYER_LOCK);
    CHECK(event.data.pending_multi_tap_effects.effect_request.feedback_pulse);
    CHECK(event.data.pending_multi_tap_effects.effect_request.feedback_long_hold_level);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
    CHECK(slot->long_hold.action == TEST_LAYER_LOCK);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_pending_multi_tap_scan_event_flushes_expired_chain(void) {
    active_key_state_t          *slot = key_runtime_primary_slot();
    key_runtime_slot_scan_event_t event;

    test_reset_state();

    slot->pending_multi_tap = (multi_tap_t){
        .keycode        = TEST_MULTI_TAP_KEY,
        .key_pos        = test_keypos(6, 2),
        .timer          = (uint16_t)(fake_time - 200),
        .count          = 2,
        .single_action  = TEST_SINGLE_ACTION,
        .multi_tap_term = 150,
    };

    event = key_runtime_slot_take_pending_multi_tap_scan_event(slot);

    CHECK(event.handled);
    CHECK(event.kind == KEY_RUNTIME_SLOT_SCAN_EVENT_PENDING_MULTI_TAP_FLUSH);
    CHECK(event.data.pending_multi_tap_flush.handled);
    CHECK(event.data.pending_multi_tap_flush.action == TEST_SINGLE_ACTION);
    CHECK(event.data.pending_multi_tap_flush.repeat_count == 2);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
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

static void test_take_pending_multi_tap_hold_release_preserves_chain_for_higher_taps(void) {
    active_key_state_t                               *slot = key_runtime_primary_slot();
    key_runtime_slot_pending_multi_tap_hold_release_t release;
    keypos_t                                          pos = test_keypos(6, 2);

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode          = TEST_MULTI_TAP_KEY,
        .key_pos          = pos,
        .tap_hold_term    = 120,
        .longer_hold_term = 240,
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

    release = key_runtime_slot_take_pending_multi_tap_hold_release(
        slot,
        TEST_MULTI_TAP_KEY,
        (key_behavior_view_t){.keycode = TEST_MULTI_TAP_KEY},
        50);

    CHECK(release.handled);
    CHECK(release.outcome == KEY_RUNTIME_SLOT_PENDING_MULTI_TAP_HOLD_RELEASE_DELAYED_ACTION);
    CHECK(release.action == KC_NO);
    CHECK(release.repeat_count == 0);
    CHECK(slot->keycode == KC_NO);
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
        KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
        KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT,
        true);

    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER);
    CHECK(request.action == TEST_HOLD_ACTION);
    CHECK(slot->keycode == TEST_ACTIVE_KEY);
    CHECK(slot->tap_action == TEST_SINGLE_ACTION);
    CHECK(slot->held_action_keycode == TEST_HOLD_ACTION);
    CHECK(key_runtime_slot_uses_implicit_hold(slot));
    CHECK(!key_runtime_slot_uses_fallback_hold(slot));
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

    plan = key_runtime_slot_take_handled_press(
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
    CHECK(key_runtime_slot_hold_is_complete(slot));
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

    test_pd_mode         = PD_MODE_VOLUME;
    test_pd_locked_modes = PD_MODE_VOLUME;

    plan = key_runtime_slot_take_handled_press(
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
    CHECK(plan.pending_multi_tap_flush.handled);
    CHECK(plan.pending_multi_tap_flush.action == TEST_SINGLE_ACTION);
    CHECK(plan.pending_multi_tap_flush.repeat_count == 1);
    CHECK(plan.reclaim_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE);
    CHECK(plan.begin_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER);
    CHECK(plan.begin_request.action == TEST_HOLD_ACTION);
    CHECK(slot->keycode == TEST_PD_MODE_KEY);
    CHECK(slot->key_pos.row == press_pos.row);
    CHECK(slot->key_pos.col == press_pos.col);
    CHECK(slot->tap_action == TEST_SINGLE_ACTION);
    CHECK(slot->held_action_keycode == TEST_HOLD_ACTION);
    CHECK(key_runtime_slot_uses_implicit_hold(slot));
    CHECK(slot->phase == KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW);
    CHECK(slot->pd_mode_was_locked_on_press);
    CHECK(!key_runtime_slot_has_pending_multi_tap(slot));
}

static void test_take_handled_release_returns_cleanup_for_unmatched_release(void) {
    key_runtime_slot_release_event_t release;
    keypos_t                         pos = test_keypos(7, 3);

    test_reset_state();
    key_runtime_primary_slot()->keycode = TEST_ACTIVE_KEY;
    key_runtime_primary_slot()->key_pos = test_keypos(7, 4);

    release = key_runtime_slot_take_handled_release(
        key_runtime_primary_slot(),
        TEST_ACTIVE_KEY,
        pos,
        (key_behavior_view_t){
            .keycode            = TEST_ACTIVE_KEY,
            .is_momentary_layer = true,
        });

    CHECK(release.handled);
    CHECK(release.kind == KEY_RUNTIME_SLOT_RELEASE_EVENT_CLEANUP);
    CHECK(release.data.cleanup.key_pos.row == pos.row);
    CHECK(release.data.cleanup.key_pos.col == pos.col);
    CHECK(release.data.cleanup.release_layer);
    CHECK(release.data.cleanup.release_owned_state);
}

static void test_interrupt_on_other_press_marks_layer_and_activates_fallback_hold(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode       = TEST_LAYER_KEY,
        .key_pos       = test_keypos(6, 2),
        .phase         = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .hold_strategy = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK,
    };

    request = key_runtime_slot_interrupt_on_other_press(slot, test_keypos(6, 3));

    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER);
    CHECK(request.action == TEST_LAYER_KEY);
    CHECK(slot->held_action_keycode == TEST_LAYER_KEY);
    CHECK(key_runtime_slot_hold_is_complete(slot));
    CHECK(slot->layer_interrupted);
}

static void test_commit_immediate_hold_sets_flags_and_feedback_request(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode = TEST_ACTIVE_KEY,
        .key_pos = test_keypos(6, 3),
        .phase   = KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
    };

    request = key_runtime_slot_commit_immediate_hold(slot, true, true);

    CHECK(request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE);
    CHECK(request.feedback_pulse);
    CHECK(!request.feedback_long_hold_level);
    CHECK(key_runtime_slot_hold_is_complete(slot));
}

static void test_take_flush_unregisters_held_action(void) {
    active_key_state_t               *slot = key_runtime_primary_slot();
    key_runtime_slot_effect_request_t request;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode             = TEST_ACTIVE_KEY,
        .key_pos             = test_keypos(6, 4),
        .held_action_keycode = TEST_HOLD_ACTION,
        .phase               = KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
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

    result = key_runtime_slot_take_handled_press_result(
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
    CHECK(result.effects[1].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_SLOT_EFFECT_REQUEST);
    CHECK(result.effects[1].key_pos.row == press_pos.row);
    CHECK(result.effects[1].key_pos.col == press_pos.col);
    CHECK(result.effects[1].data.slot_effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER);
    CHECK(result.effects[1].data.slot_effect_request.action == TEST_HOLD_ACTION);
}

static void test_take_handled_release_result_maps_pending_multi_tap_held_lifecycle(void) {
    active_key_state_t         *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t   result;
    keypos_t                    pos = test_keypos(6, 1);

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode          = TEST_MULTI_TAP_KEY,
        .key_pos          = pos,
        .tap_hold_term    = 120,
        .longer_hold_term = 240,
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

    result = key_runtime_slot_take_handled_release_result(
        slot,
        TEST_MULTI_TAP_KEY,
        pos,
        (key_behavior_view_t){.keycode = TEST_MULTI_TAP_KEY});

    CHECK(result.handled);
    CHECK(result.count == 2);
    CHECK(result.effects[0].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_SLOT_EFFECT_REQUEST);
    CHECK(result.effects[0].data.slot_effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER);
    CHECK(result.effects[0].data.slot_effect_request.action == TEST_HOLD_ACTION);
    CHECK(result.effects[1].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_SLOT_EFFECT_REQUEST);
    CHECK(result.effects[1].data.slot_effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_UNREGISTER);
    CHECK(result.effects[1].data.slot_effect_request.action == TEST_HOLD_ACTION);
}

static void test_take_active_scan_result_maps_commit_and_long_hold_requests(void) {
    active_key_state_t         *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t   result;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode             = TEST_ACTIVE_KEY,
        .key_pos             = test_keypos(3, 1),
        .phase               = KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
        .timer               = (uint16_t)(fake_time - 260),
        .tap_hold_term       = 120,
        .longer_hold_term    = 240,
        .held_action_keycode = TEST_HOLD_ACTION,
        .hold                =
            {
                .present = true,
                .action  = TEST_HOLD_ACTION,
                .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
            },
        .long_hold           = TAP_AT_HOLD_THRESHOLD(TEST_SINGLE_ACTION),
    };

    result = key_runtime_slot_take_active_scan_result(slot);

    CHECK(result.handled);
    CHECK(result.count == 2);
    CHECK(result.effects[0].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_SLOT_EFFECT_REQUEST);
    CHECK(result.effects[0].data.slot_effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_NONE);
    CHECK(result.effects[0].data.slot_effect_request.feedback_pulse);
    CHECK(result.effects[1].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_SLOT_EFFECT_REQUEST);
    CHECK(result.effects[1].data.slot_effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_DISPATCH_ACTION);
    CHECK(result.effects[1].data.slot_effect_request.action == TEST_SINGLE_ACTION);
    CHECK(result.effects[1].data.slot_effect_request.release_owned_state);
}

static void test_take_interrupt_result_maps_slot_effect_request(void) {
    active_key_state_t         *slot = key_runtime_primary_slot();
    key_runtime_slot_result_t   result;

    test_reset_state();

    *slot = (active_key_state_t){
        .keycode       = TEST_LAYER_KEY,
        .key_pos       = test_keypos(6, 2),
        .phase         = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .hold_strategy = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK,
    };

    result = key_runtime_slot_take_interrupt_result(slot, test_keypos(6, 3));

    CHECK(result.handled);
    CHECK(result.count == 1);
    CHECK(result.effects[0].kind == KEY_RUNTIME_SLOT_RESULT_EFFECT_SLOT_EFFECT_REQUEST);
    CHECK(result.effects[0].key_pos.row == slot->key_pos.row);
    CHECK(result.effects[0].key_pos.col == slot->key_pos.col);
    CHECK(result.effects[0].data.slot_effect_request.kind == KEY_RUNTIME_SLOT_EFFECT_REQUEST_HELD_REGISTER);
    CHECK(result.effects[0].data.slot_effect_request.action == TEST_LAYER_KEY);
}

int main(void) {
    test_slot_pending_multi_tap_ownership_marks_slot_non_idle();
    test_slot_pending_multi_tap_lifecycle_helpers();
    test_slot_track_preserves_pending_multi_tap();
    test_resolve_pending_multi_tap_hold_clears_slot_owned_state();
    test_resolve_release_quick_immediate_hold_becomes_tap();
    test_resolve_release_locked_pd_mode_becomes_lock_tap();
    test_take_active_release_starts_pending_multi_tap_chain();
    test_take_active_release_maps_locked_pd_mode_tap();
    test_take_active_scan_event_promotes_long_hold_after_longer_term();
    test_take_active_scan_event_returns_commit_and_long_hold_requests();
    test_take_active_scan_event_returns_fallback_hold_request();
    test_fire_hold_at_threshold_starts_repeat_binding();
    test_promote_to_long_hold_releases_owned_state_before_dispatch();
    test_take_pending_multi_tap_scan_event_returns_long_hold_request();
    test_take_pending_multi_tap_scan_event_flushes_expired_chain();
    test_take_pending_multi_tap_hold_release_returns_held_lifecycle();
    test_take_pending_multi_tap_hold_release_preserves_chain_for_higher_taps();
    test_take_pending_multi_tap_flush_prefers_exact_step_tap();
    test_begin_press_sets_metadata_and_immediate_hold_request();
    test_prepare_handled_press_matching_pending_multi_tap_reuses_slot();
    test_prepare_handled_press_flushes_pending_multi_tap_before_begin();
    test_take_handled_release_returns_cleanup_for_unmatched_release();
    test_interrupt_on_other_press_marks_layer_and_activates_fallback_hold();
    test_commit_immediate_hold_sets_flags_and_feedback_request();
    test_take_flush_unregisters_held_action();
    test_take_flush_dispatches_tap_for_unheld_non_layer_key();
    test_take_handled_press_result_maps_flush_and_begin_request();
    test_take_handled_release_result_maps_pending_multi_tap_held_lifecycle();
    test_take_active_scan_result_maps_commit_and_long_hold_requests();
    test_take_interrupt_result_maps_slot_effect_request();

    puts("key_runtime_slot host tests passed");
    return 0;
}
