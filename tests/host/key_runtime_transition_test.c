#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/runtime/key_runtime_admission.h"
#include "users/noah/lib/key/runtime/key_runtime_state.h"
#include "users/noah/lib/key/runtime/key_runtime_transition.h"
#include "users/noah/lib/state/runtime/runtime_shared_state.h"

enum {
    TEST_PLAIN_KEY           = 0x0004,
    TEST_MULTI_TAP_KEY       = SAFE_RANGE + 0x10,
    TEST_PD_MODE_KEY         = SAFE_RANGE + 0x11,
    TEST_PREVIOUS_KEY        = SAFE_RANGE + 0x12,
    TEST_NEW_KEY             = SAFE_RANGE + 0x13,
    TEST_PREVIOUS_TAP_ACTION = SAFE_RANGE + 0x14,
    TEST_MULTI_STEP_ACTION   = SAFE_RANGE + 0x15,
    TEST_IMMEDIATE_HOLD      = SAFE_RANGE + 0x16,
    TEST_THRESHOLD_HOLD      = SAFE_RANGE + 0x17,
    TEST_MULTI_TAP_HOLD      = SAFE_RANGE + 0x18,
    TEST_FALLBACK_TAP_ACTION = SAFE_RANGE + 0x19,
    TEST_LAYER_LOCK_ACTION   = LOCK_LAYER(2),
};

typedef enum {
    TEST_CALL_NONE = 0,
    TEST_CALL_DISPATCH_ACTION,
    TEST_CALL_HELD_REGISTER,
    TEST_CALL_HELD_UNREGISTER,
    TEST_CALL_RELEASE_OWNED_BY_KEY,
    TEST_CALL_REPEAT_START,
    TEST_CALL_LAYER_PRESS,
    TEST_CALL_LAYER_RELEASE,
    TEST_CALL_FEEDBACK_PULSE,
    TEST_CALL_SYNC_SPLIT_RUNTIME,
    TEST_CALL_DELAYED_ACTION,
} test_call_kind_t;

typedef struct {
    test_call_kind_t      kind;
    uint16_t              action;
    keypos_t              key_pos;
    uint8_t               layer;
    bool                  long_hold_level;
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
static pd_mode_mask_t    pd_locked_modes;
static bool              pd_toggle_result;

static bool    held_action_survives_flush_result;
static uint8_t overflow_log_count;

static test_call_t test_calls[TEST_MAX_CALLS];
static uint8_t     test_call_count;

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

static keypos_t test_default_slot_key_pos;

static void test_set_default_slot_key_pos(keypos_t key_pos) {
    test_default_slot_key_pos = key_pos;
}

static key_runtime_slot_interaction_t test_cached_interaction(uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term) {
    return key_runtime_slot_interaction_from_resolution((handled_key_resolution_t){
        .tap_action            = tap_action,
        .tap_repeat_count      = tap_action == KC_NO ? 0 : 1,
        .hold                  = hold,
        .long_hold             = long_hold,
        .hold_strategy         = KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT,
        .tap_hold_term         = tap_hold_term,
        .longer_hold_term      = longer_hold_term,
        .multi_tap_term        = multi_tap_term,
        .layer                 = UINT8_MAX,
        .pd_mode               = 0,
        .step_present          = tap_action != KC_NO || hold.present || long_hold.present,
        .has_more_taps         = false,
        .tap_resolves_on_press = false,
        .flags                 = HANDLED_KEY_FLAG_HANDLED,
    });
}

static void test_set_cached_interaction(active_key_state_t *slot, handled_key_resolution_t resolution) {
    if (!slot) {
        return;
    }

    slot->interaction.valid = true;
    slot->interaction.view  = key_runtime_slot_interaction_from_resolution(resolution);
}

#define HOLD_LIT(expr) ((hold_behavior_t)expr)

static active_key_state_t *test_slot_for_position(keypos_t key_pos) {
    return key_runtime_slot_for_position(key_pos);
}

static multi_tap_t *test_multi_tap_for_position(keypos_t key_pos) {
    return key_runtime_multi_tap_for_slot(test_slot_for_position(key_pos));
}

static active_key_state_t *test_default_slot(void) {
    return test_slot_for_position(test_default_slot_key_pos);
}

static multi_tap_t *test_default_multi_tap(void) {
    return test_multi_tap_for_position(test_default_slot_key_pos);
}

static void active_key_reset(void) {
    key_runtime_slot_reset(test_default_slot());
    key_runtime_slot_reset_pending_multi_tap(test_default_slot());
}

static void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, key_runtime_slot_phase_t phase, key_runtime_slot_hold_strategy_t hold_strategy) {
    key_runtime_slot_interaction_t interaction = key_runtime_slot_interaction_from_resolution((handled_key_resolution_t){
        .tap_action       = tap_action,
        .tap_repeat_count = tap_action == KC_NO ? 0 : 1,
        .hold             = hold,
        .long_hold        = long_hold,
        .hold_strategy    = hold_strategy,
        .tap_hold_term    = tap_hold_term,
        .longer_hold_term = longer_hold_term,
        .multi_tap_term   = multi_tap_term,
        .layer            = UINT8_MAX,
        .flags            = HANDLED_KEY_FLAG_HANDLED,
    });

    test_set_default_slot_key_pos(key_pos);
    key_runtime_slot_track(test_default_slot(), keycode, key_pos, interaction, phase);
}

#define active_key (*test_default_slot())
#define multi_tap (*test_default_multi_tap())

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

static handled_key_resolution_t test_handled_key(uint16_t keycode) {
    return handled_key_lookup(keycode);
}

static void test_handled_key_enable_multi_tap(handled_key_resolution_t *key) {
    key->flags |= HANDLED_KEY_FLAG_MULTI_TAP;
}

static void test_handled_key_enable_modifier_multi_tap(handled_key_resolution_t *key) {
    key->flags |= HANDLED_KEY_FLAG_MULTI_TAP | HANDLED_KEY_FLAG_FALLBACK_HOLD;
    key->hold_strategy = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK;
    key->tap_action    = KC_NO;
}

static void test_handled_key_set_fallback_tap(handled_key_resolution_t *key, uint16_t action) {
    key->flags |= HANDLED_KEY_FLAG_FALLBACK_HOLD;
    key->hold_strategy = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK;
    key->tap_action    = action;
}

static void test_handled_key_set_layer_contract(handled_key_resolution_t *key, uint8_t layer, bool layer_tap, uint16_t tap_action) {
    key->flags |= HANDLED_KEY_FLAG_MOMENTARY_LAYER;
    if (layer_tap) {
        key->flags |= HANDLED_KEY_FLAG_LAYER_TAP;
    } else {
        key->flags &= (uint16_t)~HANDLED_KEY_FLAG_LAYER_TAP;
    }
    key->layer      = layer;
    key->tap_action = tap_action;
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
    runtime_shared_state_reset(&noah_runtime_shared_state);
    test_set_default_slot_key_pos(test_keypos(0, 0));
    active_key_reset();
    multi_tap_reset(&multi_tap);
}

static void test_reset_stubs(void) {
    fake_time                         = 1000;
    fake_mods                         = 0;
    fake_weak_mods                    = 0;
    fake_oneshot_mods                 = 0;
    fake_oneshot_locked_mods          = 0;
    held_action_survives_flush_result = false;
    overflow_log_count                = 0;

    memset(test_calls, 0, sizeof(test_calls));
    test_call_count = 0;

    test_reset_behavior_steps();
    test_reset_pd_modes();
    test_reset_runtime();
}

int uprintf(const char *fmt, ...) {
    (void)fmt;
    overflow_log_count++;
    return 0;
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
        .handled          = true,
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

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    return (pd_locked_modes & mode) != 0;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    if (pd_toggle_result) {
        pd_locked_modes ^= mode;
    }

    return pd_toggle_result;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    (void)policy;
    test_log_call(TEST_CALL_DISPATCH_ACTION, action, test_keypos(0, 0), 0, false, (delayed_action_mods_t){0});
}

void held_action_register(keypos_t key_pos, uint16_t action) {
    test_log_call(TEST_CALL_HELD_REGISTER, action, key_pos, 0, false, (delayed_action_mods_t){0});
}

void held_action_unregister(keypos_t key_pos, uint16_t action) {
    test_log_call(TEST_CALL_HELD_UNREGISTER, action, key_pos, 0, false, (delayed_action_mods_t){0});
}

bool held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
    return held_action_survives_flush_result;
}

bool held_action_release_owned_by_key(keypos_t key_pos) {
    test_log_call(TEST_CALL_RELEASE_OWNED_BY_KEY, KC_NO, key_pos, 0, false, (delayed_action_mods_t){0});
    return true;
}

void held_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    (void)repeat_hz;
    test_log_call(TEST_CALL_REPEAT_START, action, key_pos, 0, false, (delayed_action_mods_t){0});
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    test_log_call(TEST_CALL_LAYER_PRESS, KC_NO, key_pos, layer, false, (delayed_action_mods_t){0});
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    test_log_call(TEST_CALL_LAYER_RELEASE, KC_NO, key_pos, 0, false, (delayed_action_mods_t){0});
    return true;
}

void key_feedback_pulse_arm(bool long_hold_level) {
    test_log_call(TEST_CALL_FEEDBACK_PULSE, KC_NO, test_keypos(0, 0), 0, long_hold_level, (delayed_action_mods_t){0});
}

void split_runtime_sync(void) {
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
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(plan.items[0].data.delayed_action.action == TEST_FALLBACK_TAP_ACTION);
    CHECK(plan.items[0].data.delayed_action.repeat_count == 3);
    CHECK(plan.items[0].data.delayed_action.mods.real == 0x11);
    CHECK(plan.items[0].data.delayed_action.mods.weak == 0x22);
    CHECK(plan.items[0].data.delayed_action.mods.oneshot == 0x33);
    CHECK(plan.items[0].data.delayed_action.mods.oneshot_locked == 0x44);
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
    test_add_behavior_step(TEST_MULTI_TAP_KEY, 2,
                           (key_behavior_step_t){
                               .tap = TAP_SENDS(TEST_MULTI_STEP_ACTION),
                           });

    multi_tap = (multi_tap_t){
        .keycode          = TEST_MULTI_TAP_KEY,
        .count            = 2,
        .single_action    = TEST_FALLBACK_TAP_ACTION,
        .tap_action       = TEST_MULTI_STEP_ACTION,
        .tap_repeat_count = 1,
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_flush_multi_tap(&plan);

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(plan.items[0].data.delayed_action.action == TEST_MULTI_STEP_ACTION);
    CHECK(plan.items[0].data.delayed_action.repeat_count == 1);
}

static void test_flush_multi_tap_flushes_each_active_slot_in_order(void) {
    key_runtime_transition_plan_t plan;
    multi_tap_t                  *first_multi_tap  = test_multi_tap_for_position(test_keypos(0, 0));
    multi_tap_t                  *second_multi_tap = test_multi_tap_for_position(test_keypos(0, 1));

    test_reset_stubs();
    *first_multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .key_pos       = test_keypos(0, 0),
        .count         = 1,
        .single_action = TEST_PREVIOUS_TAP_ACTION,
    };
    *second_multi_tap = (multi_tap_t){
        .keycode       = KC_RIGHT_ALT,
        .key_pos       = test_keypos(0, 1),
        .count         = 2,
        .single_action = TEST_FALLBACK_TAP_ACTION,
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_flush_multi_tap(&plan);

    CHECK(plan.count == 2);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(plan.items[0].data.delayed_action.action == TEST_PREVIOUS_TAP_ACTION);
    CHECK(plan.items[0].data.delayed_action.repeat_count == 1);
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(plan.items[1].data.delayed_action.action == TEST_FALLBACK_TAP_ACTION);
    CHECK(plan.items[1].data.delayed_action.repeat_count == 2);
    CHECK(first_multi_tap->keycode == KC_NO);
    CHECK(second_multi_tap->keycode == KC_NO);
}

static void test_quick_release_locked_pd_mode_queues_lock_tap(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(2, 3), false);
    handled_key_resolution_t            key;

    test_reset_stubs();
    test_add_pd_mode_mapping(TEST_PD_MODE_KEY, PD_MODE_VOLUME);
    pd_locked_modes = PD_MODE_VOLUME;
    key             = test_handled_key(TEST_PD_MODE_KEY);
    test_set_default_slot_key_pos(record.event.key);

    active_key = (active_key_state_t){
        .timer                                 = (uint16_t)(fake_time - 50),
        .owner.keycode                         = TEST_PD_MODE_KEY,
        .owner.key_pos                         = record.event.key,
        .lifecycle.held_action_keycode         = TEST_PD_MODE_KEY,
        .lifecycle.pd_mode_was_locked_on_press = true,
        .interaction.valid                     = true,
        .interaction.view                      = test_cached_interaction(KC_NO, (hold_behavior_t){
                                                                   .present = true,
                                                                   .action  = TEST_PD_MODE_KEY,
                                                                   .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                                                               }, hold_behavior_none(), 150, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
    };
    handled_key_resolution_t pd_mode_resolution = key_runtime_slot_interaction_to_resolution(active_key.interaction.view);
    pd_mode_resolution.pd_mode                  = PD_MODE_VOLUME;
    test_set_cached_interaction(test_default_slot(), pd_mode_resolution);

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_PD_MODE_KEY, &record, key, &plan));

    CHECK(plan.count == 2);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(plan.items[0].data.key_pos.row == record.event.key.row);
    CHECK(plan.items[0].data.key_pos.col == record.event.key.col);
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP);
    CHECK(plan.items[1].data.pd_mode == PD_MODE_VOLUME);
    CHECK(active_key.owner.keycode == KC_NO);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_RELEASE_OWNED_BY_KEY);
    CHECK(test_calls[1].kind == TEST_CALL_SYNC_SPLIT_RUNTIME);
    CHECK((pd_locked_modes & PD_MODE_VOLUME) == 0);
}

static void test_quick_release_immediate_hold_unregisters_then_taps(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(3, 4), false);
    handled_key_resolution_t            key    = test_handled_key(TEST_NEW_KEY);

    test_reset_stubs();
    test_set_default_slot_key_pos(record.event.key);
    active_key = (active_key_state_t){
        .timer                         = (uint16_t)(fake_time - 50),
        .owner.keycode                 = TEST_NEW_KEY,
        .owner.key_pos                 = record.event.key,
        .lifecycle.held_action_keycode = TEST_IMMEDIATE_HOLD,
        .interaction.valid             = true,
        .interaction.view              = test_cached_interaction(TEST_FALLBACK_TAP_ACTION, (hold_behavior_t){
                                                                   .present = true,
                                                                   .action  = TEST_IMMEDIATE_HOLD,
                                                                   .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                                                               }, hold_behavior_none(), 150, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_NEW_KEY, &record, key, &plan));

    CHECK(plan.count == 2);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(plan.items[0].data.key_pos.row == record.event.key.row);
    CHECK(plan.items[0].data.key_pos.col == record.event.key.col);
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(plan.items[1].data.action == TEST_FALLBACK_TAP_ACTION);
    CHECK(active_key.owner.keycode == KC_NO);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_RELEASE_OWNED_BY_KEY);
    CHECK(test_calls[1].kind == TEST_CALL_DISPATCH_ACTION);
    CHECK(test_calls[1].action == TEST_FALLBACK_TAP_ACTION);
}

static void test_modifier_multi_tap_first_tap_is_buffered(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   press_record   = test_record(test_keypos(4, 4), true);
    keyrecord_t                   release_record = test_record(test_keypos(4, 4), false);
    handled_key_resolution_t            key            = test_handled_key(KC_RIGHT_ALT);

    test_reset_stubs();
    test_set_default_slot_key_pos(press_record.event.key);
    test_handled_key_enable_modifier_multi_tap(&key);

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_press(key_runtime_select_slot_for_press(press_record.event.key), KC_RIGHT_ALT, press_record.event.key, key, false, &plan));

    CHECK(plan.count == 0);
    CHECK(active_key.owner.keycode == KC_RIGHT_ALT);
    CHECK(!key_runtime_slot_uses_implicit_hold(&active_key));
    CHECK(key_runtime_slot_uses_fallback_hold(&active_key));
    CHECK(!active_key.interaction.view.resolution.hold.present);
    CHECK(active_key.interaction.view.resolution.tap_action == KC_NO);

    fake_time = (uint16_t)(fake_time + 50);

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(KC_RIGHT_ALT, &release_record, key, &plan));

    CHECK(plan.count == 0);
    CHECK(active_key.owner.keycode == KC_NO);
    CHECK(multi_tap.keycode == KC_RIGHT_ALT);
    CHECK(multi_tap.count == 1);
    CHECK(multi_tap.single_action == KC_NO);
    CHECK(test_call_count == 0);
}

static void test_single_tap_override_activates_fallback_hold_at_threshold(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   press_record = test_record(test_keypos(4, 5), true);
    handled_key_resolution_t            key          = test_handled_key(KC_RIGHT_ALT);

    test_reset_stubs();
    test_set_default_slot_key_pos(press_record.event.key);
    test_handled_key_set_fallback_tap(&key, TEST_FALLBACK_TAP_ACTION);

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_press(key_runtime_select_slot_for_press(press_record.event.key), KC_RIGHT_ALT, press_record.event.key, key, false, &plan));

    CHECK(plan.count == 0);
    CHECK(key_runtime_slot_uses_fallback_hold(&active_key));
    CHECK(active_key.interaction.view.resolution.tap_action == TEST_FALLBACK_TAP_ACTION);
    CHECK(active_key.lifecycle.held_action_keycode == KC_NO);

    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM + 10);

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.items[0].data.held_action.key_pos.row == press_record.event.key.row);
    CHECK(plan.items[0].data.held_action.key_pos.col == press_record.event.key.col);
    CHECK(plan.items[0].data.held_action.action == KC_RIGHT_ALT);
    CHECK(active_key.lifecycle.held_action_keycode == KC_RIGHT_ALT);
    CHECK(key_runtime_slot_hold_is_complete(&active_key));
}

static void test_single_tap_override_long_release_does_not_dispatch_tap(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   press_record   = test_record(test_keypos(4, 5), true);
    keyrecord_t                   release_record = test_record(test_keypos(4, 5), false);
    handled_key_resolution_t            key            = test_handled_key(KC_RIGHT_ALT);

    test_reset_stubs();
    test_set_default_slot_key_pos(press_record.event.key);
    test_handled_key_set_fallback_tap(&key, TEST_FALLBACK_TAP_ACTION);

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_press(key_runtime_select_slot_for_press(press_record.event.key), KC_RIGHT_ALT, press_record.event.key, key, false, &plan));

    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM + 10);

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(KC_RIGHT_ALT, &release_record, key, &plan));

    CHECK(plan.count == 0);
    CHECK(active_key.owner.keycode == KC_NO);
    CHECK(test_call_count == 0);
}

static void test_non_modifier_single_tap_override_activates_fallback_hold_at_threshold(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   press_record = test_record(test_keypos(4, 6), true);
    handled_key_resolution_t            key          = test_handled_key(TEST_PLAIN_KEY);

    test_reset_stubs();
    test_set_default_slot_key_pos(press_record.event.key);
    test_handled_key_set_fallback_tap(&key, TEST_FALLBACK_TAP_ACTION);

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_press(key_runtime_select_slot_for_press(press_record.event.key), TEST_PLAIN_KEY, press_record.event.key, key, false, &plan));

    CHECK(plan.count == 0);
    CHECK(key_runtime_slot_uses_fallback_hold(&active_key));
    CHECK(active_key.interaction.view.resolution.tap_action == TEST_FALLBACK_TAP_ACTION);
    CHECK(active_key.lifecycle.held_action_keycode == KC_NO);

    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM + 10);

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.items[0].data.held_action.key_pos.row == press_record.event.key.row);
    CHECK(plan.items[0].data.held_action.key_pos.col == press_record.event.key.col);
    CHECK(plan.items[0].data.held_action.action == TEST_PLAIN_KEY);
    CHECK(active_key.lifecycle.held_action_keycode == TEST_PLAIN_KEY);
    CHECK(key_runtime_slot_hold_is_complete(&active_key));
}

static void test_interrupt_other_press_queues_pending_fallback_hold(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(2, 2));
    active_key = (active_key_state_t){
        .owner.keycode           = TEST_PLAIN_KEY,
        .owner.key_pos           = test_keypos(2, 2),
        .lifecycle.phase         = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
        .interaction.valid       = true,
        .interaction.view        = test_cached_interaction(KC_NO, hold_behavior_none(), hold_behavior_none(), CUSTOM_TAP_HOLD_TERM, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
    };
    handled_key_resolution_t fallback_resolution = key_runtime_slot_interaction_to_resolution(active_key.interaction.view);
    fallback_resolution.hold_strategy            = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK;
    fallback_resolution.flags |= HANDLED_KEY_FLAG_FALLBACK_HOLD;
    test_set_cached_interaction(test_default_slot(), fallback_resolution);

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_interrupt_active_key_on_other_press(&plan);

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.items[0].data.held_action.key_pos.row == active_key.owner.key_pos.row);
    CHECK(plan.items[0].data.held_action.key_pos.col == active_key.owner.key_pos.col);
    CHECK(plan.items[0].data.held_action.action == TEST_PLAIN_KEY);
    CHECK(active_key.lifecycle.held_action_keycode == TEST_PLAIN_KEY);
    CHECK(key_runtime_slot_hold_is_complete(&active_key));

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 1);
    CHECK(test_calls[0].kind == TEST_CALL_HELD_REGISTER);
    CHECK(test_calls[0].action == TEST_PLAIN_KEY);
}

static void test_interrupt_other_press_marks_layer_interrupted(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(2, 5));
    active_key = (active_key_state_t){
        .owner.keycode = LT(2, TEST_FALLBACK_TAP_ACTION),
        .owner.key_pos = test_keypos(2, 5),
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_interrupt_active_key_on_other_press(&plan);

    CHECK(plan.count == 0);
    CHECK(active_key.lifecycle.layer_interrupted);
}

static void test_modifier_multi_tap_second_tap_dispatches_action(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   press_record_1   = test_record(test_keypos(4, 4), true);
    keyrecord_t                   release_record_1 = test_record(test_keypos(4, 4), false);
    keyrecord_t                   press_record_2   = test_record(test_keypos(4, 4), true);
    handled_key_resolution_t            key              = test_handled_key(KC_RIGHT_ALT);

    test_reset_stubs();
    test_set_default_slot_key_pos(press_record_1.event.key);
    test_handled_key_enable_modifier_multi_tap(&key);
    test_add_behavior_step(KC_RIGHT_ALT, 2,
                           (key_behavior_step_t){
                               .tap = TAP_SENDS(TEST_MULTI_STEP_ACTION),
                           });

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_press(key_runtime_select_slot_for_press(press_record_1.event.key), KC_RIGHT_ALT, press_record_1.event.key, key, false, &plan));
    CHECK(plan.count == 0);

    fake_time = (uint16_t)(fake_time + 50);
    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(KC_RIGHT_ALT, &release_record_1, key, &plan));
    CHECK(plan.count == 0);
    CHECK(multi_tap.keycode == KC_RIGHT_ALT);
    CHECK(multi_tap.count == 1);

    fake_time = (uint16_t)(fake_time + 50);
    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_press(key_runtime_select_slot_for_press(press_record_2.event.key), KC_RIGHT_ALT, press_record_2.event.key, key, false, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(plan.items[0].data.action == TEST_MULTI_STEP_ACTION);
    CHECK(multi_tap.keycode == KC_NO);
    CHECK(active_key.owner.keycode == KC_NO);
}

static void test_interrupted_momentary_layer_release_only_releases_layer(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record            = test_record(test_keypos(1, 5), false);
    handled_key_resolution_t            key               = test_handled_key(LT(2, TEST_FALLBACK_TAP_ACTION));
    uint16_t                      layer_tap_keycode = LT(2, TEST_FALLBACK_TAP_ACTION);

    test_reset_stubs();
    test_handled_key_set_layer_contract(&key, 2, true, TEST_FALLBACK_TAP_ACTION);
    test_set_default_slot_key_pos(record.event.key);

    active_key = (active_key_state_t){
        .timer                       = (uint16_t)(fake_time - 30),
        .owner.keycode               = layer_tap_keycode,
        .owner.key_pos               = record.event.key,
        .interaction.valid           = true,
        .interaction.view            = test_cached_interaction(TEST_FALLBACK_TAP_ACTION, hold_behavior_none(), hold_behavior_none(), 150, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
        .lifecycle.layer_interrupted = true,
    };
    handled_key_resolution_t interrupted_layer_resolution = key_runtime_slot_interaction_to_resolution(active_key.interaction.view);
    interrupted_layer_resolution.layer                    = 2;
    interrupted_layer_resolution.flags |= HANDLED_KEY_FLAG_MOMENTARY_LAYER | HANDLED_KEY_FLAG_LAYER_TAP;
    test_set_cached_interaction(test_default_slot(), interrupted_layer_resolution);

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(layer_tap_keycode, &record, key, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_LAYER_RELEASE);
    CHECK(plan.items[0].data.key_pos.row == record.event.key.row);
    CHECK(plan.items[0].data.key_pos.col == record.event.key.col);
    CHECK(active_key.owner.keycode == KC_NO);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 1);
    CHECK(test_calls[0].kind == TEST_CALL_LAYER_RELEASE);
    CHECK(test_calls[0].key_pos.row == record.event.key.row);
    CHECK(test_calls[0].key_pos.col == record.event.key.col);
}

static void test_release_hold_prefers_long_hold_after_longer_term(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(2, 1), false);
    handled_key_resolution_t            key    = test_handled_key(TEST_NEW_KEY);

    test_reset_stubs();
    test_set_default_slot_key_pos(record.event.key);
    active_key = (active_key_state_t){
        .timer                   = (uint16_t)(fake_time - 260),
        .owner.keycode           = TEST_NEW_KEY,
        .owner.key_pos           = record.event.key,
        .interaction.valid       = true,
        .interaction.view        = test_cached_interaction(TEST_FALLBACK_TAP_ACTION, HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_THRESHOLD_HOLD)), HOLD_LIT(TAP_ON_RELEASE_AFTER_HOLD(TEST_MULTI_STEP_ACTION)), 120, 240, CUSTOM_MULTI_TAP_TERM),
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_NEW_KEY, &record, key, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(plan.items[0].data.action == TEST_MULTI_STEP_ACTION);
}

static void test_release_repeat_hold_releases_owned_state(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(2, 2), false);
    handled_key_resolution_t            key    = test_handled_key(TEST_NEW_KEY);

    test_reset_stubs();
    test_set_default_slot_key_pos(record.event.key);
    active_key = (active_key_state_t){
        .timer                           = (uint16_t)(fake_time - 160),
        .owner.keycode                   = TEST_NEW_KEY,
        .owner.key_pos                   = record.event.key,
        .lifecycle.phase                 = KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
        .lifecycle.repeat_binding_active = true,
        .interaction.valid               = true,
        .interaction.view                = test_cached_interaction(KC_NO, HOLD_LIT(REPEAT_WHILE_HELD(TEST_THRESHOLD_HOLD, 25)), hold_behavior_none(), 120, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_NEW_KEY, &record, key, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(plan.items[0].data.key_pos.row == record.event.key.row);
    CHECK(plan.items[0].data.key_pos.col == record.event.key.col);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 1);
    CHECK(test_calls[0].kind == TEST_CALL_RELEASE_OWNED_BY_KEY);
}

static void test_mismatched_release_releases_owned_held_action(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(7, 7), false);
    handled_key_resolution_t            key    = test_handled_key(TEST_NEW_KEY);

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(7, 6));
    active_key = (active_key_state_t){
        .owner.keycode = TEST_NEW_KEY,
        .owner.key_pos = test_keypos(7, 6),
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_NEW_KEY, &record, key, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(plan.items[0].data.key_pos.row == record.event.key.row);
    CHECK(plan.items[0].data.key_pos.col == record.event.key.col);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 1);
    CHECK(test_calls[0].kind == TEST_CALL_RELEASE_OWNED_BY_KEY);
    CHECK(test_calls[0].key_pos.row == record.event.key.row);
    CHECK(test_calls[0].key_pos.col == record.event.key.col);
}

static void test_press_on_different_position_preserves_existing_active_state(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record   = test_record(test_keypos(4, 1), true);
    handled_key_resolution_t            key      = test_handled_key(TEST_NEW_KEY);
    active_key_state_t           *new_slot = test_slot_for_position(record.event.key);

    test_reset_stubs();
    active_key_track(TEST_PREVIOUS_KEY, test_keypos(0, 1), TEST_PREVIOUS_TAP_ACTION, hold_behavior_none(), hold_behavior_none(), CUSTOM_TAP_HOLD_TERM, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM, KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW, KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT);

    key.hold = (hold_behavior_t){
        .present = true,
        .action  = TEST_IMMEDIATE_HOLD,
        .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_press(key_runtime_select_slot_for_press(record.event.key), TEST_NEW_KEY, record.event.key, key, false, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.items[0].data.held_action.action == TEST_IMMEDIATE_HOLD);
    CHECK(active_key.owner.keycode == TEST_PREVIOUS_KEY);
    CHECK(active_key.interaction.view.resolution.tap_action == TEST_PREVIOUS_TAP_ACTION);
    CHECK(new_slot->owner.keycode == TEST_NEW_KEY);
    CHECK(new_slot->owner.key_pos.row == record.event.key.row);
    CHECK(new_slot->owner.key_pos.col == record.event.key.col);
    CHECK(new_slot->lifecycle.held_action_keycode == TEST_IMMEDIATE_HOLD);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 1);
    CHECK(test_calls[0].kind == TEST_CALL_HELD_REGISTER);
    CHECK(test_calls[0].action == TEST_IMMEDIATE_HOLD);
}

static void test_press_on_third_position_keeps_existing_positions_active(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record     = test_record(test_keypos(4, 1), true);
    handled_key_resolution_t            key        = test_handled_key(TEST_NEW_KEY);
    active_key_state_t           *other_slot = test_slot_for_position(test_keypos(0, 2));
    active_key_state_t           *new_slot   = test_slot_for_position(record.event.key);

    test_reset_stubs();
    active_key_track(TEST_PREVIOUS_KEY, test_keypos(0, 1), TEST_PREVIOUS_TAP_ACTION, hold_behavior_none(), hold_behavior_none(), CUSTOM_TAP_HOLD_TERM, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM, KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW, KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT);
    *other_slot = (active_key_state_t){
        .timer                         = fake_time,
        .owner.keycode                 = TEST_PLAIN_KEY,
        .owner.key_pos                 = test_keypos(0, 2),
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
        .interaction.valid             = true,
        .interaction.view              = test_cached_interaction(TEST_FALLBACK_TAP_ACTION, hold_behavior_none(), hold_behavior_none(), CUSTOM_TAP_HOLD_TERM, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
        .lifecycle.held_action_keycode = TEST_PLAIN_KEY,
    };

    key.hold = (hold_behavior_t){
        .present = true,
        .action  = TEST_IMMEDIATE_HOLD,
        .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_press(key_runtime_select_slot_for_press(record.event.key), TEST_NEW_KEY, record.event.key, key, false, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.items[0].data.held_action.action == TEST_IMMEDIATE_HOLD);
    CHECK(active_key.owner.keycode == TEST_PREVIOUS_KEY);
    CHECK(other_slot->owner.keycode == TEST_PLAIN_KEY);
    CHECK(new_slot->owner.keycode == TEST_NEW_KEY);
    CHECK(new_slot->owner.key_pos.row == record.event.key.row);
    CHECK(new_slot->owner.key_pos.col == record.event.key.col);
    CHECK(new_slot->lifecycle.held_action_keycode == TEST_IMMEDIATE_HOLD);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 1);
    CHECK(test_calls[0].kind == TEST_CALL_HELD_REGISTER);
    CHECK(test_calls[0].action == TEST_IMMEDIATE_HOLD);
}

static void test_release_on_other_position_leaves_existing_position_active(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record        = test_record(test_keypos(4, 2), false);
    handled_key_resolution_t            key           = test_handled_key(TEST_NEW_KEY);
    active_key_state_t           *released_slot = test_slot_for_position(record.event.key);

    test_reset_stubs();
    active_key_track(TEST_PREVIOUS_KEY, test_keypos(0, 1), TEST_PREVIOUS_TAP_ACTION, hold_behavior_none(), hold_behavior_none(), CUSTOM_TAP_HOLD_TERM, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM, KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW, KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT);
    *released_slot = (active_key_state_t){
        .timer                = (uint16_t)(fake_time - 40),
        .owner.keycode        = TEST_NEW_KEY,
        .owner.key_pos        = record.event.key,
        .interaction.valid    = true,
        .interaction.view     = test_cached_interaction(TEST_FALLBACK_TAP_ACTION, hold_behavior_none(), hold_behavior_none(), 120, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_NEW_KEY, &record, key, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(plan.items[0].data.action == TEST_FALLBACK_TAP_ACTION);
    CHECK(active_key.owner.keycode == TEST_PREVIOUS_KEY);
    CHECK(released_slot->owner.keycode == KC_NO);
}

static void test_release_on_other_position_starts_independent_multi_tap_chain(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record         = test_record(test_keypos(4, 2), false);
    handled_key_resolution_t            key            = test_handled_key(TEST_MULTI_TAP_KEY);
    multi_tap_t                  *previous_chain = test_multi_tap_for_position(test_keypos(0, 1));
    active_key_state_t           *released_slot  = test_slot_for_position(record.event.key);

    test_reset_stubs();
    test_handled_key_enable_multi_tap(&key);
    *previous_chain = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .key_pos       = test_keypos(0, 1),
        .count         = 1,
        .single_action = TEST_PREVIOUS_TAP_ACTION,
    };
    *released_slot = (active_key_state_t){
        .timer                 = (uint16_t)(fake_time - 40),
        .owner.keycode         = TEST_MULTI_TAP_KEY,
        .owner.key_pos         = record.event.key,
        .interaction.valid     = true,
        .interaction.view      = test_cached_interaction(TEST_FALLBACK_TAP_ACTION, hold_behavior_none(), hold_behavior_none(), 120, CUSTOM_LONGER_HOLD_TERM, 150),
    };
    handled_key_resolution_t pending_tap_resolution = key_runtime_slot_interaction_to_resolution(released_slot->interaction.view);
    pending_tap_resolution.flags |= HANDLED_KEY_FLAG_MULTI_TAP;
    pending_tap_resolution.has_more_taps            = true;
    test_set_cached_interaction(released_slot, pending_tap_resolution);

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_MULTI_TAP_KEY, &record, key, &plan));

    CHECK(plan.count == 0);
    CHECK(released_slot->owner.keycode == KC_NO);
    CHECK(previous_chain->keycode == TEST_MULTI_TAP_KEY);
    CHECK(previous_chain->count == 1);
    CHECK(released_slot->pending_multi_tap.keycode == TEST_MULTI_TAP_KEY);
    CHECK(released_slot->pending_multi_tap.key_pos.row == record.event.key.row);
    CHECK(released_slot->pending_multi_tap.key_pos.col == record.event.key.col);
    CHECK(released_slot->pending_multi_tap.count == 1);
    CHECK(released_slot->pending_multi_tap.single_action == TEST_FALLBACK_TAP_ACTION);
}

static void test_press_on_new_position_preserves_existing_pending_multi_tap(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record                 = test_record(test_keypos(4, 3), true);
    handled_key_resolution_t            key                    = test_handled_key(TEST_NEW_KEY);
    multi_tap_t                  *pending_slot_multi_tap = test_multi_tap_for_position(test_keypos(0, 2));
    active_key_state_t           *new_slot               = test_slot_for_position(record.event.key);

    test_reset_stubs();
    active_key_track(TEST_PREVIOUS_KEY, test_keypos(0, 1), TEST_PREVIOUS_TAP_ACTION, hold_behavior_none(), hold_behavior_none(), CUSTOM_TAP_HOLD_TERM, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM, KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW, KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT);
    *pending_slot_multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .key_pos       = test_keypos(0, 2),
        .count         = 1,
        .single_action = TEST_FALLBACK_TAP_ACTION,
    };

    key.hold = (hold_behavior_t){
        .present = true,
        .action  = TEST_IMMEDIATE_HOLD,
        .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_press(key_runtime_select_slot_for_press(record.event.key), TEST_NEW_KEY, record.event.key, key, false, &plan));

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.items[0].data.held_action.action == TEST_IMMEDIATE_HOLD);
    CHECK(active_key.owner.keycode == TEST_PREVIOUS_KEY);
    CHECK(new_slot->owner.keycode == TEST_NEW_KEY);
    CHECK(new_slot->lifecycle.held_action_keycode == TEST_IMMEDIATE_HOLD);
    CHECK(pending_slot_multi_tap->keycode == TEST_MULTI_TAP_KEY);
    CHECK(pending_slot_multi_tap->single_action == TEST_FALLBACK_TAP_ACTION);
}

static void test_scan_fires_hold_for_independent_position_slot(void) {
    key_runtime_transition_plan_t plan;
    active_key_state_t           *other_slot = test_slot_for_position(test_keypos(6, 3));

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(1, 1));
    active_key = (active_key_state_t){
        .timer           = fake_time,
        .owner.keycode   = TEST_PREVIOUS_KEY,
        .owner.key_pos   = test_keypos(1, 1),
        .lifecycle.phase = KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
    };
    *other_slot = (active_key_state_t){
        .timer                = (uint16_t)(fake_time - 170),
        .owner.keycode        = TEST_NEW_KEY,
        .owner.key_pos        = test_keypos(6, 3),
        .interaction.valid    = true,
        .interaction.view     = test_cached_interaction(KC_NO, (hold_behavior_t){
                                                           .present   = true,
                                                           .action    = TEST_THRESHOLD_HOLD,
                                                           .repeat_hz = 25,
                                                           .mode      = HOLD_BEHAVIOR_REPEAT_WHILE_HELD,
                                                       }, hold_behavior_none(), 120, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 2);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_REPEAT_START);
    CHECK(plan.items[0].data.repeat.key_pos.row == other_slot->owner.key_pos.row);
    CHECK(plan.items[0].data.repeat.key_pos.col == other_slot->owner.key_pos.col);
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(other_slot->lifecycle.repeat_binding_active);
    CHECK(key_runtime_slot_hold_is_complete(other_slot));
}

static void test_release_pending_multi_tap_hold_registers_then_unregisters_held_action(void) {
    key_runtime_transition_plan_t plan;
    keyrecord_t                   record = test_record(test_keypos(6, 2), false);
    handled_key_resolution_t            key    = test_handled_key(TEST_MULTI_TAP_KEY);

    test_reset_stubs();
    test_set_default_slot_key_pos(record.event.key);
    active_key = (active_key_state_t){
        .timer                = (uint16_t)(fake_time - 200),
        .owner.keycode        = TEST_MULTI_TAP_KEY,
        .owner.key_pos        = record.event.key,
        .interaction.valid    = true,
        .interaction.view     = test_cached_interaction(KC_NO, HOLD_LIT(PRESS_AND_HOLD_UNTIL_RELEASE(TEST_MULTI_TAP_HOLD)), hold_behavior_none(), 120, 240, CUSTOM_MULTI_TAP_TERM),
    };

    multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .key_pos       = record.event.key,
        .timer         = (uint16_t)(fake_time - 150),
        .count         = 2,
        .pending_hold  = true,
        .tap_hold_term = 120,
        .hold          = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_MULTI_TAP_HOLD),
        .long_hold     = hold_behavior_none(),
    };

    key_runtime_transition_plan_init(&plan);
    CHECK(key_runtime_transition_handled_key_release(TEST_MULTI_TAP_KEY, &record, key, &plan));

    CHECK(plan.count == 2);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.items[0].data.held_action.key_pos.row == record.event.key.row);
    CHECK(plan.items[0].data.held_action.key_pos.col == record.event.key.col);
    CHECK(plan.items[0].data.held_action.action == TEST_MULTI_TAP_HOLD);
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER);
    CHECK(plan.items[1].data.held_action.key_pos.row == record.event.key.row);
    CHECK(plan.items[1].data.held_action.key_pos.col == record.event.key.col);
    CHECK(plan.items[1].data.held_action.action == TEST_MULTI_TAP_HOLD);
    CHECK(active_key.owner.keycode == KC_NO);
    CHECK(multi_tap.keycode == KC_NO);
    CHECK(multi_tap.count == 0);

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_HELD_REGISTER);
    CHECK(test_calls[0].action == TEST_MULTI_TAP_HOLD);
    CHECK(test_calls[1].kind == TEST_CALL_HELD_UNREGISTER);
    CHECK(test_calls[1].action == TEST_MULTI_TAP_HOLD);
}

static void test_quick_release_pending_multi_tap_hold_keeps_chain_alive_for_layer_key(void) {
    key_runtime_transition_plan_t release_plan;
    key_runtime_transition_plan_t scan_plan;
    keyrecord_t                   record = test_record(test_keypos(6, 2), false);
    handled_key_resolution_t            key    = test_handled_key(MO(2));

    test_reset_stubs();
    test_handled_key_enable_multi_tap(&key);
    test_handled_key_set_layer_contract(&key, 2, false, KC_NO);
    key.tap_hold_term    = 120;
    key.longer_hold_term = 240;
    key.multi_tap_term   = 150;
    test_add_behavior_step(MO(2), 2,
                           (key_behavior_step_t){
                               .tap       = TAP_SENDS(TEST_MULTI_STEP_ACTION),
                               .long_hold = TAP_AT_HOLD_THRESHOLD(TEST_LAYER_LOCK_ACTION),
                           });
    test_add_behavior_step(MO(2), 3,
                           (key_behavior_step_t){
                               .tap = TAP_SENDS(TEST_PREVIOUS_TAP_ACTION),
                           });
    test_set_default_slot_key_pos(record.event.key);

    active_key = (active_key_state_t){
        .timer                = (uint16_t)(fake_time - 50),
        .owner.keycode        = MO(2),
        .owner.key_pos        = record.event.key,
        .interaction.valid    = true,
        .interaction.view     = test_cached_interaction(KC_NO, hold_behavior_none(), HOLD_LIT(TAP_AT_HOLD_THRESHOLD(TEST_LAYER_LOCK_ACTION)), 120, 240, CUSTOM_MULTI_TAP_TERM),
    };
    handled_key_resolution_t layered_multi_tap_resolution = key_runtime_slot_interaction_to_resolution(active_key.interaction.view);
    layered_multi_tap_resolution.flags |= HANDLED_KEY_FLAG_MULTI_TAP | HANDLED_KEY_FLAG_MOMENTARY_LAYER;
    layered_multi_tap_resolution.has_more_taps = true;
    layered_multi_tap_resolution.layer         = 2;
    test_set_cached_interaction(test_default_slot(), layered_multi_tap_resolution);

    multi_tap = (multi_tap_t){
        .keycode          = MO(2),
        .key_pos          = record.event.key,
        .timer            = (uint16_t)(fake_time - 50),
        .count            = 2,
        .pending_hold     = true,
        .tap_action       = TEST_MULTI_STEP_ACTION,
        .tap_repeat_count = 1,
        .single_action    = TEST_FALLBACK_TAP_ACTION,
        .has_more_taps    = true,
        .tap_hold_term    = 120,
        .multi_tap_term   = 150,
        .long_hold        = TAP_AT_HOLD_THRESHOLD(TEST_LAYER_LOCK_ACTION),
    };

    key_runtime_transition_plan_init(&release_plan);
    CHECK(key_runtime_transition_handled_key_release(MO(2), &record, key, &release_plan));

    CHECK(release_plan.count == 1);
    CHECK(release_plan.items[0].kind == KEY_RUNTIME_EFFECT_LAYER_RELEASE);
    CHECK(active_key.owner.keycode == KC_NO);
    CHECK(multi_tap.keycode == MO(2));
    CHECK(multi_tap.count == 2);
    CHECK(!multi_tap.pending_hold);

    fake_time = (uint16_t)(fake_time + 200);

    key_runtime_transition_plan_init(&scan_plan);
    key_runtime_transition_scan(&scan_plan);

    CHECK(scan_plan.count == 1);
    CHECK(scan_plan.items[0].kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(scan_plan.items[0].data.delayed_action.action == TEST_MULTI_STEP_ACTION);
    CHECK(scan_plan.items[0].data.delayed_action.repeat_count == 1);
}

static void test_scan_promotes_pending_multi_tap_hold(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(6, 2));
    active_key = (active_key_state_t){
        .timer                = (uint16_t)(fake_time - 200),
        .owner.keycode        = TEST_MULTI_TAP_KEY,
        .owner.key_pos        = test_keypos(6, 2),
        .interaction.valid    = true,
        .interaction.view     = test_cached_interaction(KC_NO, HOLD_LIT(PRESS_AND_HOLD_UNTIL_RELEASE(TEST_MULTI_TAP_HOLD)), hold_behavior_none(), 120, 240, CUSTOM_MULTI_TAP_TERM),
    };

    multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .key_pos       = test_keypos(6, 2),
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
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.items[0].data.held_action.action == TEST_MULTI_TAP_HOLD);
    CHECK(active_key.lifecycle.held_action_keycode == TEST_MULTI_TAP_HOLD);
    CHECK(key_runtime_slot_hold_is_complete(&active_key));
    CHECK(multi_tap.keycode == KC_NO);
    CHECK(multi_tap.count == 0);
}

static void test_scan_flushes_expired_pending_multi_tap_chain(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(6, 4));
    multi_tap = (multi_tap_t){
        .keycode        = TEST_MULTI_TAP_KEY,
        .key_pos        = test_keypos(6, 4),
        .timer          = (uint16_t)(fake_time - 200),
        .count          = 2,
        .single_action  = TEST_FALLBACK_TAP_ACTION,
        .multi_tap_term = 150,
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(plan.items[0].data.delayed_action.action == TEST_FALLBACK_TAP_ACTION);
    CHECK(plan.items[0].data.delayed_action.repeat_count == 2);
    CHECK(multi_tap.keycode == KC_NO);
    CHECK(multi_tap.count == 0);
}

static void test_scan_starts_repeat_hold_at_threshold(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(6, 3));
    active_key = (active_key_state_t){
        .timer                = (uint16_t)(fake_time - 170),
        .owner.keycode        = TEST_NEW_KEY,
        .owner.key_pos        = test_keypos(6, 3),
        .interaction.valid    = true,
        .interaction.view     = test_cached_interaction(KC_NO, HOLD_LIT(REPEAT_WHILE_HELD(TEST_THRESHOLD_HOLD, 25)), hold_behavior_none(), 120, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 2);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_REPEAT_START);
    CHECK(plan.items[0].data.repeat.key_pos.row == active_key.owner.key_pos.row);
    CHECK(plan.items[0].data.repeat.key_pos.col == active_key.owner.key_pos.col);
    CHECK(plan.items[0].data.repeat.action == TEST_THRESHOLD_HOLD);
    CHECK(plan.items[0].data.repeat.repeat_hz == 25);
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(!plan.items[1].data.long_hold_level);
    CHECK(active_key.lifecycle.repeat_binding_active);
    CHECK(key_runtime_slot_hold_is_complete(&active_key));

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 2);
    CHECK(test_calls[0].kind == TEST_CALL_REPEAT_START);
    CHECK(test_calls[0].action == TEST_THRESHOLD_HOLD);
    CHECK(test_calls[1].kind == TEST_CALL_FEEDBACK_PULSE);
}

static void test_scan_commits_immediate_hold_threshold_with_feedback(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(5, 0));
    active_key = (active_key_state_t){
        .timer                         = (uint16_t)(fake_time - 170),
        .owner.keycode                 = TEST_NEW_KEY,
        .owner.key_pos                 = test_keypos(5, 0),
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
        .lifecycle.held_action_keycode = TEST_IMMEDIATE_HOLD,
        .interaction.valid             = true,
        .interaction.view              = test_cached_interaction(KC_NO, (hold_behavior_t){
                                                                   .present = true,
                                                                   .action  = TEST_IMMEDIATE_HOLD,
                                                                   .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                                                               }, hold_behavior_none(), 120, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(!plan.items[0].data.long_hold_level);
    CHECK(key_runtime_slot_hold_is_complete(&active_key));

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 1);
    CHECK(test_calls[0].kind == TEST_CALL_FEEDBACK_PULSE);
    CHECK(!test_calls[0].long_hold_level);
}

static void test_scan_commits_implicit_hold_without_feedback(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(5, 1));
    active_key = (active_key_state_t){
        .timer                         = (uint16_t)(fake_time - 170),
        .owner.keycode                 = TEST_PD_MODE_KEY,
        .owner.key_pos                 = test_keypos(5, 1),
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
        .lifecycle.held_action_keycode = TEST_PD_MODE_KEY,
        .interaction.valid             = true,
        .interaction.view              = test_cached_interaction(KC_NO, (hold_behavior_t){
                                                                   .present = true,
                                                                   .action  = TEST_PD_MODE_KEY,
                                                                   .mode    = HOLD_BEHAVIOR_PRESS_IMMEDIATELY_UNTIL_RELEASE,
                                                               }, hold_behavior_none(), 120, CUSTOM_LONGER_HOLD_TERM, CUSTOM_MULTI_TAP_TERM),
    };
    handled_key_resolution_t implicit_hold_resolution = key_runtime_slot_interaction_to_resolution(active_key.interaction.view);
    implicit_hold_resolution.hold_strategy            = KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT;
    implicit_hold_resolution.flags |= HANDLED_KEY_FLAG_IMPLICIT_HOLD;
    test_set_cached_interaction(test_default_slot(), implicit_hold_resolution);

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 0);
    CHECK(key_runtime_slot_hold_is_complete(&active_key));
    CHECK(test_call_count == 0);
}

static void test_scan_promotes_to_long_hold_and_replaces_held_action(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(3, 0));
    active_key = (active_key_state_t){
        .timer                         = (uint16_t)(fake_time - 260),
        .owner.keycode                 = TEST_NEW_KEY,
        .owner.key_pos                 = test_keypos(3, 0),
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE,
        .lifecycle.held_action_keycode = TEST_THRESHOLD_HOLD,
        .interaction.valid             = true,
        .interaction.view              = test_cached_interaction(KC_NO, HOLD_LIT(PRESS_AND_HOLD_UNTIL_RELEASE(TEST_THRESHOLD_HOLD)), HOLD_LIT(TAP_AT_HOLD_THRESHOLD(TEST_MULTI_STEP_ACTION)), 120, 240, CUSTOM_MULTI_TAP_TERM),
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 3);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(plan.items[0].data.key_pos.row == active_key.owner.key_pos.row);
    CHECK(plan.items[0].data.key_pos.col == active_key.owner.key_pos.col);
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(plan.items[1].data.action == TEST_MULTI_STEP_ACTION);
    CHECK(plan.items[2].kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(plan.items[2].data.long_hold_level);
    CHECK(active_key.lifecycle.held_action_keycode == KC_NO);
    CHECK(key_runtime_slot_hold_is_complete(&active_key));

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 3);
    CHECK(test_calls[0].kind == TEST_CALL_RELEASE_OWNED_BY_KEY);
    CHECK(test_calls[1].kind == TEST_CALL_DISPATCH_ACTION);
    CHECK(test_calls[1].action == TEST_MULTI_STEP_ACTION);
    CHECK(test_calls[2].kind == TEST_CALL_FEEDBACK_PULSE);
    CHECK(test_calls[2].long_hold_level);
}

static void test_scan_promotes_repeat_hold_to_long_hold(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(3, 1));
    active_key = (active_key_state_t){
        .timer                           = (uint16_t)(fake_time - 260),
        .owner.keycode                   = TEST_NEW_KEY,
        .owner.key_pos                   = test_keypos(3, 1),
        .lifecycle.phase                 = KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE,
        .lifecycle.repeat_binding_active = true,
        .interaction.valid               = true,
        .interaction.view                = test_cached_interaction(KC_NO, HOLD_LIT(REPEAT_WHILE_HELD(TEST_THRESHOLD_HOLD, 25)), HOLD_LIT(TAP_AT_HOLD_THRESHOLD(TEST_MULTI_STEP_ACTION)), 120, 240, CUSTOM_MULTI_TAP_TERM),
    };

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_scan(&plan);

    CHECK(plan.count == 3);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
    CHECK(plan.items[0].data.key_pos.row == active_key.owner.key_pos.row);
    CHECK(plan.items[0].data.key_pos.col == active_key.owner.key_pos.col);
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(plan.items[1].data.action == TEST_MULTI_STEP_ACTION);
    CHECK(plan.items[2].kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(plan.items[2].data.long_hold_level);
    CHECK(!active_key.lifecycle.repeat_binding_active);
    CHECK(key_runtime_slot_hold_is_complete(&active_key));

    key_runtime_transition_execute_plan(&plan);
    CHECK(test_call_count == 3);
    CHECK(test_calls[0].kind == TEST_CALL_RELEASE_OWNED_BY_KEY);
    CHECK(test_calls[1].kind == TEST_CALL_DISPATCH_ACTION);
    CHECK(test_calls[1].action == TEST_MULTI_STEP_ACTION);
    CHECK(test_calls[2].kind == TEST_CALL_FEEDBACK_PULSE);
    CHECK(test_calls[2].long_hold_level);
}

static void test_scan_pending_multi_tap_long_hold_releases_layer_before_lock(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();
    test_set_default_slot_key_pos(test_keypos(6, 5));
    active_key = (active_key_state_t){
        .timer                   = (uint16_t)(fake_time - 260),
        .owner.keycode           = LT(1, TEST_FALLBACK_TAP_ACTION),
        .owner.key_pos           = test_keypos(6, 5),
        .interaction.valid       = true,
        .interaction.view        = test_cached_interaction(KC_NO, hold_behavior_none(), HOLD_LIT(TAP_AT_HOLD_THRESHOLD(TEST_LAYER_LOCK_ACTION)), 120, 240, CUSTOM_MULTI_TAP_TERM),
    };

    multi_tap = (multi_tap_t){
        .keycode       = TEST_MULTI_TAP_KEY,
        .key_pos       = active_key.owner.key_pos,
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
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_LAYER_RELEASE);
    CHECK(plan.items[0].data.key_pos.row == active_key.owner.key_pos.row);
    CHECK(plan.items[0].data.key_pos.col == active_key.owner.key_pos.col);
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(plan.items[1].data.action == TEST_LAYER_LOCK_ACTION);
    CHECK(plan.items[2].kind == KEY_RUNTIME_EFFECT_FEEDBACK_PULSE);
    CHECK(plan.items[2].data.long_hold_level);
    CHECK(key_runtime_slot_hold_is_complete(&active_key));
    CHECK(multi_tap.keycode == KC_NO);
    CHECK(multi_tap.count == 0);
}

static void test_interrupt_plan_overflow_sets_flag_and_logs_once(void) {
    key_runtime_transition_plan_t plan;

    test_reset_stubs();

    for (uint8_t index = 0; index < (uint8_t)(KEY_RUNTIME_TRANSITION_PLAN_CAPACITY + 2); index++) {
        keypos_t            key_pos = test_keypos((uint8_t)(index / MATRIX_COLS), (uint8_t)(index % MATRIX_COLS));
        active_key_state_t *slot    = test_slot_for_position(key_pos);

        *slot = (active_key_state_t){
            .owner.keycode           = (uint16_t)(TEST_NEW_KEY + index),
            .owner.key_pos           = key_pos,
            .lifecycle.phase         = KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
            .interaction.valid       = true,
            .interaction.view        = key_runtime_slot_interaction_from_resolution((handled_key_resolution_t){
                .tap_action    = KC_NO,
                .hold_strategy = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK,
                .layer         = UINT8_MAX,
                .pd_mode       = 0,
                .flags         = HANDLED_KEY_FLAG_HANDLED | HANDLED_KEY_FLAG_FALLBACK_HOLD,
            }),
        };
    }

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_interrupt_active_keys_on_other_press(test_keypos(7, 7), &plan);

    CHECK(plan.count == KEY_RUNTIME_TRANSITION_PLAN_CAPACITY);
    CHECK(plan.overflowed);
    CHECK(overflow_log_count == 1);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.items[0].data.held_action.action == TEST_NEW_KEY);
    CHECK(plan.items[KEY_RUNTIME_TRANSITION_PLAN_CAPACITY - 1].kind == KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER);
    CHECK(plan.items[KEY_RUNTIME_TRANSITION_PLAN_CAPACITY - 1].data.held_action.action == (uint16_t)(TEST_NEW_KEY + KEY_RUNTIME_TRANSITION_PLAN_CAPACITY - 1));
}

int main(void) {
    test_flush_multi_tap_replays_single_action();
    test_flush_multi_tap_prefers_exact_step_tap();
    test_flush_multi_tap_flushes_each_active_slot_in_order();
    test_quick_release_locked_pd_mode_queues_lock_tap();
    test_quick_release_immediate_hold_unregisters_then_taps();
    test_modifier_multi_tap_first_tap_is_buffered();
    test_single_tap_override_activates_fallback_hold_at_threshold();
    test_single_tap_override_long_release_does_not_dispatch_tap();
    test_non_modifier_single_tap_override_activates_fallback_hold_at_threshold();
    test_interrupt_other_press_queues_pending_fallback_hold();
    test_interrupt_other_press_marks_layer_interrupted();
    test_modifier_multi_tap_second_tap_dispatches_action();
    test_interrupted_momentary_layer_release_only_releases_layer();
    test_release_hold_prefers_long_hold_after_longer_term();
    test_release_repeat_hold_releases_owned_state();
    test_mismatched_release_releases_owned_held_action();
    test_press_on_different_position_preserves_existing_active_state();
    test_press_on_third_position_keeps_existing_positions_active();
    test_release_on_other_position_leaves_existing_position_active();
    test_release_on_other_position_starts_independent_multi_tap_chain();
    test_press_on_new_position_preserves_existing_pending_multi_tap();
    test_scan_fires_hold_for_independent_position_slot();
    test_release_pending_multi_tap_hold_registers_then_unregisters_held_action();
    test_quick_release_pending_multi_tap_hold_keeps_chain_alive_for_layer_key();
    test_scan_promotes_pending_multi_tap_hold();
    test_scan_flushes_expired_pending_multi_tap_chain();
    test_scan_starts_repeat_hold_at_threshold();
    test_scan_commits_immediate_hold_threshold_with_feedback();
    test_scan_commits_implicit_hold_without_feedback();
    test_scan_promotes_to_long_hold_and_replaces_held_action();
    test_scan_promotes_repeat_hold_to_long_hold();
    test_scan_pending_multi_tap_long_hold_releases_layer_before_lock();
    test_interrupt_plan_overflow_sets_flag_and_logs_once();

    puts("key_runtime_transition host tests passed");
    return 0;
}
