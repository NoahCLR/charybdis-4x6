#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/key_runtime_state.h"
#include "users/noah/lib/key/key_runtime_transition.h"

enum {
    TEST_MULTI_TAP_KEY        = SAFE_RANGE + 0x10,
    TEST_PD_MODE_KEY          = SAFE_RANGE + 0x11,
    TEST_PREVIOUS_KEY         = SAFE_RANGE + 0x12,
    TEST_NEW_KEY              = SAFE_RANGE + 0x13,
    TEST_PREVIOUS_TAP_ACTION  = SAFE_RANGE + 0x14,
    TEST_MULTI_STEP_ACTION    = SAFE_RANGE + 0x15,
    TEST_IMMEDIATE_HOLD       = SAFE_RANGE + 0x16,
    TEST_THRESHOLD_HOLD       = SAFE_RANGE + 0x17,
    TEST_MULTI_TAP_HOLD       = SAFE_RANGE + 0x18,
    TEST_FALLBACK_TAP_ACTION  = SAFE_RANGE + 0x19,
    TEST_LAYER_LOCK_ACTION    = SAFE_RANGE + 0x1A,
};

typedef enum {
    TEST_CALL_NONE = 0,
    TEST_CALL_DISPATCH_ACTION,
    TEST_CALL_HELD_REGISTER,
    TEST_CALL_HELD_UNREGISTER,
    TEST_CALL_RELEASE_HELD_OWNED_BY_KEY,
    TEST_CALL_LAYER_PRESS,
    TEST_CALL_LAYER_RELEASE,
    TEST_CALL_FEEDBACK_PULSE,
    TEST_CALL_SYNC_SPLIT_RUNTIME,
    TEST_CALL_DELAYED_ACTION,
} test_call_kind_t;

typedef struct {
    test_call_kind_t     kind;
    uint16_t             action;
    keypos_t             key_pos;
    uint8_t              layer;
    bool                 long_hold_level;
    delayed_action_mods_t mods;
} test_call_t;

typedef struct {
    uint16_t            keycode;
    uint8_t             tap_count;
    key_behavior_step_t step;
} behavior_step_stub_t;

typedef struct {
    uint16_t       keycode;
    pd_mode_mask_t mode;
} pd_mode_mapping_t;

#define TEST_MAX_BEHAVIOR_STEPS 8
#define TEST_MAX_PD_MODE_MAPPINGS 8
#define TEST_MAX_CALLS 32

static uint16_t fake_time;
static uint8_t  fake_mods;
static uint8_t  fake_weak_mods;
static uint8_t  fake_oneshot_mods;
static uint8_t  fake_oneshot_locked_mods;

static behavior_step_stub_t behavior_steps[TEST_MAX_BEHAVIOR_STEPS];
static uint8_t              behavior_step_count;

static pd_mode_mapping_t pd_mode_mappings[TEST_MAX_PD_MODE_MAPPINGS];
static uint8_t           pd_mode_mapping_count;
static pd_mode_mask_t    pd_lockable_modes;
static pd_mode_mask_t    pd_locked_modes;
static bool              pd_toggle_result;

static bool key_runtime_effects_survives_flush;

static test_call_t test_calls[TEST_MAX_CALLS];
static uint8_t     test_call_count;

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

static handled_key_view_t test_handled_key(uint16_t keycode) {
    return (handled_key_view_t){
        .behavior =
            {
                .keycode          = keycode,
                .handled          = true,
                .tap_hold_term    = CUSTOM_TAP_HOLD_TERM,
                .longer_hold_term = CUSTOM_LONGER_HOLD_TERM,
                .multi_tap_term   = CUSTOM_MULTI_TAP_TERM,
            },
    };
}

static void test_log_call(test_call_kind_t kind, uint16_t action, keypos_t key_pos, uint8_t layer, bool long_hold_level, delayed_action_mods_t mods) {
    CHECK(test_call_count < ARRAY_SIZE(test_calls));
    test_calls[test_call_count++] = (test_call_t){
        .kind            = kind,
        .action          = action,
        .key_pos         = key_pos,
        .layer           = layer,
        .long_hold_level = long_hold_level,
        .mods            = mods,
    };
}

static void test_reset_behavior_steps(void) {
    memset(behavior_steps, 0, sizeof(behavior_steps));
    behavior_step_count = 0;
}

static void test_add_behavior_step(uint16_t keycode, uint8_t tap_count, key_behavior_step_t step) {
    CHECK(behavior_step_count < ARRAY_SIZE(behavior_steps));
    behavior_steps[behavior_step_count++] = (behavior_step_stub_t){
        .keycode   = keycode,
        .tap_count = tap_count,
        .step      = step,
    };
}

static void test_reset_pd_modes(void) {
    memset(pd_mode_mappings, 0, sizeof(pd_mode_mappings));
    pd_mode_mapping_count = 0;
    pd_lockable_modes     = 0;
    pd_locked_modes       = 0;
    pd_toggle_result      = true;
}

static void test_add_pd_mode_mapping(uint16_t keycode, pd_mode_mask_t mode) {
    CHECK(pd_mode_mapping_count < ARRAY_SIZE(pd_mode_mappings));
    pd_mode_mappings[pd_mode_mapping_count++] = (pd_mode_mapping_t){
        .keycode = keycode,
        .mode    = mode,
    };
}

static void test_reset_runtime(void) {
    noah_runtime_shared_state = (runtime_shared_state_t){0};
    active_key_reset();
    multi_tap_reset(&multi_tap);
}

static void test_reset_stubs(void) {
    fake_time                          = 1000;
    fake_mods                          = 0;
    fake_weak_mods                     = 0;
    fake_oneshot_mods                  = 0;
    fake_oneshot_locked_mods           = 0;
    key_runtime_effects_survives_flush = false;

    memset(test_calls, 0, sizeof(test_calls));
    test_call_count = 0;

    test_reset_behavior_steps();
    test_reset_pd_modes();
    test_reset_runtime();
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
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

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    return (key_behavior_view_t){
        .keycode          = keycode,
        .tap_hold_term    = CUSTOM_TAP_HOLD_TERM,
        .longer_hold_term = CUSTOM_LONGER_HOLD_TERM,
        .multi_tap_term   = CUSTOM_MULTI_TAP_TERM,
    };
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    for (uint8_t i = 0; i < behavior_step_count; i++) {
        if (behavior_steps[i].keycode == keycode && behavior_steps[i].tap_count == tap_count) {
            return behavior_steps[i].step;
        }
    }

    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    for (uint8_t i = 0; i < behavior_step_count; i++) {
        if (behavior_steps[i].keycode == keycode && behavior_steps[i].tap_count > count && key_behavior_step_present(behavior_steps[i].step)) {
            return true;
        }
    }

    return false;
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    test_log_call(TEST_CALL_DELAYED_ACTION, action, test_keypos(0, 0), 0, false, mods);
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < pd_mode_mapping_count; i++) {
        if (pd_mode_mappings[i].keycode == keycode) {
            return pd_mode_mappings[i].mode;
        }
    }

    return 0;
}

bool pd_mode_locked(pd_mode_mask_t mode) {
    return (pd_locked_modes & mode) != 0;
}

bool pd_mode_is_lockable(pd_mode_mask_t mode) {
    return (pd_lockable_modes & mode) != 0;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    if (!pd_mode_is_lockable(mode)) {
        return false;
    }

    if (pd_toggle_result) {
        pd_locked_modes ^= mode;
    }

    return pd_toggle_result;
}

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    (void)action;
    return NOAH_ACTION_HOLD_KIND_SHARED;
}

bool action_dispatch_is_layer_action(uint16_t action) {
    return IS_QK_MOMENTARY(action) || IS_QK_LAYER_TAP(action);
}

bool action_dispatch_is_layer_lock(uint16_t action) {
    return action == TEST_LAYER_LOCK_ACTION;
}

bool key_runtime_effects_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void key_runtime_effects_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

void key_runtime_effects_dispatch_action(uint16_t action) {
    test_log_call(TEST_CALL_DISPATCH_ACTION, action, test_keypos(0, 0), 0, false, (delayed_action_mods_t){0});
}

void key_runtime_effects_held_action_register(keypos_t key_pos, uint16_t action) {
    test_log_call(TEST_CALL_HELD_REGISTER, action, key_pos, 0, false, (delayed_action_mods_t){0});
}

void key_runtime_effects_held_action_unregister(keypos_t key_pos, uint16_t action) {
    test_log_call(TEST_CALL_HELD_UNREGISTER, action, key_pos, 0, false, (delayed_action_mods_t){0});
}

bool key_runtime_effects_held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
    return key_runtime_effects_survives_flush;
}

bool key_runtime_effects_release_held_action_owned_by_key(keypos_t key_pos) {
    test_log_call(TEST_CALL_RELEASE_HELD_OWNED_BY_KEY, KC_NO, key_pos, 0, false, (delayed_action_mods_t){0});
    return true;
}

void key_runtime_effects_layer_press(keypos_t key_pos, uint8_t layer) {
    test_log_call(TEST_CALL_LAYER_PRESS, KC_NO, key_pos, layer, false, (delayed_action_mods_t){0});
}

void key_runtime_effects_layer_release(keypos_t key_pos) {
    test_log_call(TEST_CALL_LAYER_RELEASE, KC_NO, key_pos, 0, false, (delayed_action_mods_t){0});
}

void key_runtime_effects_feedback_pulse_arm(bool long_hold_level) {
    test_log_call(TEST_CALL_FEEDBACK_PULSE, KC_NO, test_keypos(0, 0), 0, long_hold_level, (delayed_action_mods_t){0});
}

void key_runtime_effects_sync_split_runtime(void) {
    test_log_call(TEST_CALL_SYNC_SPLIT_RUNTIME, KC_NO, test_keypos(0, 0), 0, false, (delayed_action_mods_t){0});
}

static void test_flush_multi_tap_replays_single_action(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    multi_tap = (multi_tap_t){
        .keycode                   = TEST_MULTI_TAP_KEY,
        .count                     = 3,
        .single_action             = TEST_FALLBACK_TAP_ACTION,
        .saved_mods                = 0x11,
        .saved_weak_mods           = 0x22,
        .saved_oneshot_mods        = 0x33,
        .saved_oneshot_locked_mods = 0x44,
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_flush_multi_tap(&plan);

    CHECK(plan.count == 1);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_DELAYED_ACTION);
    CHECK(plan.effects[0].data.delayed_action.action == TEST_FALLBACK_TAP_ACTION);
    CHECK(plan.effects[0].data.delayed_action.repeat_count == 3);
    CHECK(plan.effects[0].data.delayed_action.mods.real == 0x11);
    CHECK(plan.effects[0].data.delayed_action.mods.weak == 0x22);
    CHECK(plan.effects[0].data.delayed_action.mods.oneshot == 0x33);
    CHECK(plan.effects[0].data.delayed_action.mods.oneshot_locked == 0x44);
    CHECK(multi_tap.keycode == KC_NO);
    CHECK(multi_tap.count == 0);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 3);
    CHECK(test_calls[0].kind == TEST_CALL_DELAYED_ACTION);
    CHECK(test_calls[1].kind == TEST_CALL_DELAYED_ACTION);
    CHECK(test_calls[2].kind == TEST_CALL_DELAYED_ACTION);
}

static void test_flush_multi_tap_prefers_exact_step_tap(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_add_behavior_step(TEST_MULTI_TAP_KEY, 2, (key_behavior_step_t){
                                                  .tap = TAP_SENDS(TEST_MULTI_STEP_ACTION),
                                              });

    multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .count         = 2,
        .single_action = TEST_FALLBACK_TAP_ACTION,
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_flush_multi_tap(&plan);

    CHECK(plan.count == 1);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_DELAYED_ACTION);
    CHECK(plan.effects[0].data.delayed_action.action == TEST_MULTI_STEP_ACTION);
    CHECK(plan.effects[0].data.delayed_action.repeat_count == 1);
}

static void test_quick_release_locked_pd_mode_queues_lock_tap(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(2, 3), false);
    handled_key_view_t            key    = test_handled_key(TEST_PD_MODE_KEY);

    test_reset_stubs();
    test_add_pd_mode_mapping(TEST_PD_MODE_KEY, PD_MODE_VOLUME);
    pd_lockable_modes = PD_MODE_VOLUME;
    pd_locked_modes   = PD_MODE_VOLUME;

    active_key = (active_key_state_t){
        .timer                       = (uint16_t)(fake_time - 50),
        .keycode                     = TEST_PD_MODE_KEY,
        .key_pos                     = record.event.key,
        .held_action_keycode         = TEST_PD_MODE_KEY,
        .tap_hold_term               = 150,
        .pd_mode_was_locked_on_press = true,
        .hold                        = {
            .present = true,
            .action  = TEST_PD_MODE_KEY,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        },
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_PD_MODE_KEY, &record, key, &plan));

    CHECK(plan.count == 2);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_UNREGISTER);
    CHECK(plan.effects[0].data.held_action.action == TEST_PD_MODE_KEY);
    CHECK(plan.effects[1].kind == KEY_RUNTIME_TRANSITION_EFFECT_PD_MODE_LOCK_TAP);
    CHECK(plan.effects[1].data.pd_mode == PD_MODE_VOLUME);
    CHECK(active_key.keycode == KC_NO);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_HELD_UNREGISTER);
    CHECK(test_calls[1].kind == TEST_CALL_SYNC_SPLIT_RUNTIME);
    CHECK((pd_locked_modes & PD_MODE_VOLUME) == 0);
}

static void test_quick_release_immediate_hold_unregisters_then_taps(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(3, 4), false);
    handled_key_view_t            key    = test_handled_key(TEST_NEW_KEY);

    test_reset_stubs();
    active_key = (active_key_state_t){
        .timer               = (uint16_t)(fake_time - 50),
        .keycode             = TEST_NEW_KEY,
        .key_pos             = record.event.key,
        .held_action_keycode = TEST_IMMEDIATE_HOLD,
        .tap_action          = TEST_FALLBACK_TAP_ACTION,
        .tap_hold_term       = 150,
        .hold                = {
            .present = true,
            .action  = TEST_IMMEDIATE_HOLD,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        },
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_NEW_KEY, &record, key, &plan));

    CHECK(plan.count == 2);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_UNREGISTER);
    CHECK(plan.effects[0].data.held_action.action == TEST_IMMEDIATE_HOLD);
    CHECK(plan.effects[1].kind == KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION);
    CHECK(plan.effects[1].data.action == TEST_FALLBACK_TAP_ACTION);
    CHECK(active_key.keycode == KC_NO);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_HELD_UNREGISTER);
    CHECK(test_calls[0].action == TEST_IMMEDIATE_HOLD);
    CHECK(test_calls[1].kind == TEST_CALL_DISPATCH_ACTION);
    CHECK(test_calls[1].action == TEST_FALLBACK_TAP_ACTION);
}

static void test_interrupted_momentary_layer_release_only_releases_layer(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(1, 5), false);
    handled_key_view_t            key    = test_handled_key(LT(2, TEST_FALLBACK_TAP_ACTION));

    test_reset_stubs();
    key.behavior.is_momentary_layer = true;
    key.behavior.is_layer_tap       = true;

    active_key = (active_key_state_t){
        .timer             = (uint16_t)(fake_time - 30),
        .keycode           = key.behavior.keycode,
        .key_pos           = record.event.key,
        .tap_action        = TEST_FALLBACK_TAP_ACTION,
        .tap_hold_term     = 150,
        .layer_interrupted = true,
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(key.behavior.keycode, &record, key, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_LAYER_RELEASE);
    CHECK(plan.effects[0].data.key_pos.row == record.event.key.row);
    CHECK(plan.effects[0].data.key_pos.col == record.event.key.col);
    CHECK(active_key.keycode == KC_NO);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 1);
    CHECK(test_calls[0].kind == TEST_CALL_LAYER_RELEASE);
    CHECK(test_calls[0].key_pos.row == record.event.key.row);
    CHECK(test_calls[0].key_pos.col == record.event.key.col);
}

static void test_release_hold_prefers_long_hold_after_longer_term(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(2, 1), false);
    handled_key_view_t            key    = test_handled_key(TEST_NEW_KEY);

    test_reset_stubs();
    active_key = (active_key_state_t){
        .timer            = (uint16_t)(fake_time - 260),
        .keycode          = TEST_NEW_KEY,
        .key_pos          = record.event.key,
        .tap_action       = TEST_FALLBACK_TAP_ACTION,
        .tap_hold_term    = 120,
        .longer_hold_term = 240,
        .hold             = TAP_ON_RELEASE_AFTER_HOLD(TEST_THRESHOLD_HOLD),
        .long_hold        = TAP_ON_RELEASE_AFTER_HOLD(TEST_MULTI_STEP_ACTION),
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_NEW_KEY, &record, key, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION);
    CHECK(plan.effects[0].data.action == TEST_MULTI_STEP_ACTION);
}

static void test_mismatched_release_releases_owned_held_action(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(7, 7), false);
    handled_key_view_t            key    = test_handled_key(TEST_NEW_KEY);

    test_reset_stubs();
    active_key = (active_key_state_t){
        .keycode = TEST_NEW_KEY,
        .key_pos = test_keypos(7, 6),
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_NEW_KEY, &record, key, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_RELEASE_HELD_ACTION_OWNED_BY_KEY);
    CHECK(plan.effects[0].data.key_pos.row == record.event.key.row);
    CHECK(plan.effects[0].data.key_pos.col == record.event.key.col);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 1);
    CHECK(test_calls[0].kind == TEST_CALL_RELEASE_HELD_OWNED_BY_KEY);
    CHECK(test_calls[0].key_pos.row == record.event.key.row);
    CHECK(test_calls[0].key_pos.col == record.event.key.col);
}

static void test_press_flushes_previous_tap_and_registers_immediate_hold(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(4, 1), true);
    handled_key_view_t            key    = test_handled_key(TEST_NEW_KEY);

    test_reset_stubs();
    active_key_track(TEST_PREVIOUS_KEY, test_keypos(0, 1), TEST_PREVIOUS_TAP_ACTION, hold_behavior_none(), hold_behavior_none(), CUSTOM_TAP_HOLD_TERM, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM, false);

    key.behavior.single.hold = (hold_behavior_t){
        .present = true,
        .action  = TEST_IMMEDIATE_HOLD,
        .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_press(TEST_NEW_KEY, &record, key, false, &plan));

    CHECK(plan.count == 2);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION);
    CHECK(plan.effects[0].data.action == TEST_PREVIOUS_TAP_ACTION);
    CHECK(plan.effects[1].kind == KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.effects[1].data.held_action.action == TEST_IMMEDIATE_HOLD);
    CHECK(active_key.keycode == TEST_NEW_KEY);
    CHECK(active_key.key_pos.row == record.event.key.row);
    CHECK(active_key.key_pos.col == record.event.key.col);
    CHECK(active_key.held_action_keycode == TEST_IMMEDIATE_HOLD);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_DISPATCH_ACTION);
    CHECK(test_calls[0].action == TEST_PREVIOUS_TAP_ACTION);
    CHECK(test_calls[1].kind == TEST_CALL_HELD_REGISTER);
    CHECK(test_calls[1].action == TEST_IMMEDIATE_HOLD);
}

static void test_scan_promotes_pending_multi_tap_hold(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    active_key = (active_key_state_t){
        .timer            = (uint16_t)(fake_time - 200),
        .keycode          = TEST_MULTI_TAP_KEY,
        .key_pos          = test_keypos(6, 2),
        .tap_hold_term    = 120,
        .longer_hold_term = 240,
    };

    multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .timer         = (uint16_t)(fake_time - 150),
        .count         = 2,
        .pending_hold  = true,
        .tap_hold_term = 120,
        .hold          = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_MULTI_TAP_HOLD),
        .long_hold     = hold_behavior_none(),
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 1);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.effects[0].data.held_action.action == TEST_MULTI_TAP_HOLD);
    CHECK(active_key.held_action_keycode == TEST_MULTI_TAP_HOLD);
    CHECK(active_key.hold_fired);
    CHECK(multi_tap.keycode == KC_NO);
    CHECK(multi_tap.count == 0);
}

static void test_scan_commits_immediate_hold_threshold_with_feedback(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    active_key = (active_key_state_t){
        .timer               = (uint16_t)(fake_time - 170),
        .keycode             = TEST_NEW_KEY,
        .key_pos             = test_keypos(5, 0),
        .held_action_keycode = TEST_IMMEDIATE_HOLD,
        .tap_hold_term       = 120,
        .hold                = {
            .present = true,
            .action  = TEST_IMMEDIATE_HOLD,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        },
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 1);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_FEEDBACK_PULSE);
    CHECK(!plan.effects[0].data.long_hold_level);
    CHECK(active_key.hold_one_shot_fired);
    CHECK(active_key.hold_fired);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 1);
    CHECK(test_calls[0].kind == TEST_CALL_FEEDBACK_PULSE);
    CHECK(!test_calls[0].long_hold_level);
}

static void test_scan_commits_implicit_pd_mode_hold_without_feedback(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    active_key = (active_key_state_t){
        .timer                 = (uint16_t)(fake_time - 170),
        .keycode               = TEST_PD_MODE_KEY,
        .key_pos               = test_keypos(5, 1),
        .held_action_keycode   = TEST_PD_MODE_KEY,
        .tap_hold_term         = 120,
        .implicit_pd_mode_hold = true,
        .hold                  = {
            .present = true,
            .action  = TEST_PD_MODE_KEY,
            .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
        },
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 0);
    CHECK(active_key.hold_one_shot_fired);
    CHECK(active_key.hold_fired);
    CHECK(test_call_count == 0);
}

static void test_scan_promotes_to_long_hold_and_replaces_held_action(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    active_key = (active_key_state_t){
        .timer               = (uint16_t)(fake_time - 260),
        .keycode             = TEST_NEW_KEY,
        .key_pos             = test_keypos(3, 0),
        .held_action_keycode = TEST_THRESHOLD_HOLD,
        .tap_hold_term       = 120,
        .longer_hold_term    = 240,
        .hold                = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_THRESHOLD_HOLD),
        .long_hold           = TAP_AT_HOLD_THRESHOLD(TEST_MULTI_STEP_ACTION),
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 3);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_HELD_ACTION_UNREGISTER);
    CHECK(plan.effects[0].data.held_action.action == TEST_THRESHOLD_HOLD);
    CHECK(plan.effects[1].kind == KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION);
    CHECK(plan.effects[1].data.action == TEST_MULTI_STEP_ACTION);
    CHECK(plan.effects[2].kind == KEY_RUNTIME_TRANSITION_EFFECT_FEEDBACK_PULSE);
    CHECK(plan.effects[2].data.long_hold_level);
    CHECK(active_key.held_action_keycode == KC_NO);
    CHECK(active_key.hold_fired);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 3);
    CHECK(test_calls[0].kind == TEST_CALL_HELD_UNREGISTER);
    CHECK(test_calls[1].kind == TEST_CALL_DISPATCH_ACTION);
    CHECK(test_calls[1].action == TEST_MULTI_STEP_ACTION);
    CHECK(test_calls[2].kind == TEST_CALL_FEEDBACK_PULSE);
    CHECK(test_calls[2].long_hold_level);
}

static void test_scan_pending_multi_tap_long_hold_releases_layer_before_lock(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    active_key = (active_key_state_t){
        .timer            = (uint16_t)(fake_time - 260),
        .keycode          = LT(1, TEST_FALLBACK_TAP_ACTION),
        .key_pos          = test_keypos(6, 5),
        .tap_hold_term    = 120,
        .longer_hold_term = 240,
    };

    multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .timer         = (uint16_t)(fake_time - 250),
        .count         = 2,
        .pending_hold  = true,
        .tap_hold_term = 120,
        .hold          = hold_behavior_none(),
        .long_hold     = TAP_AT_HOLD_THRESHOLD(TEST_LAYER_LOCK_ACTION),
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 3);
    CHECK(plan.effects[0].kind == KEY_RUNTIME_TRANSITION_EFFECT_LAYER_RELEASE);
    CHECK(plan.effects[0].data.key_pos.row == active_key.key_pos.row);
    CHECK(plan.effects[0].data.key_pos.col == active_key.key_pos.col);
    CHECK(plan.effects[1].kind == KEY_RUNTIME_TRANSITION_EFFECT_DISPATCH_ACTION);
    CHECK(plan.effects[1].data.action == TEST_LAYER_LOCK_ACTION);
    CHECK(plan.effects[2].kind == KEY_RUNTIME_TRANSITION_EFFECT_FEEDBACK_PULSE);
    CHECK(plan.effects[2].data.long_hold_level);
    CHECK(active_key.hold_fired);
    CHECK(multi_tap.keycode == KC_NO);
    CHECK(multi_tap.count == 0);
}

int main(void) {
    test_flush_multi_tap_replays_single_action();
    test_flush_multi_tap_prefers_exact_step_tap();
    test_quick_release_locked_pd_mode_queues_lock_tap();
    test_quick_release_immediate_hold_unregisters_then_taps();
    test_interrupted_momentary_layer_release_only_releases_layer();
    test_release_hold_prefers_long_hold_after_longer_term();
    test_mismatched_release_releases_owned_held_action();
    test_press_flushes_previous_tap_and_registers_immediate_hold();
    test_scan_promotes_pending_multi_tap_hold();
    test_scan_commits_immediate_hold_threshold_with_feedback();
    test_scan_commits_implicit_pd_mode_hold_without_feedback();
    test_scan_promotes_to_long_hold_and_replaces_held_action();
    test_scan_pending_multi_tap_long_hold_releases_layer_before_lock();

    puts("key_runtime_transition host tests passed");
    return 0;
}
