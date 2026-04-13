#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/runtime/key_runtime_feedback.h"
#include "users/noah/lib/key/runtime/slot/key_runtime_slot_step.h"
#include "users/noah/lib/key/runtime/key_runtime_state.h"

enum {
    TEST_MULTI_TAP_KEY      = SAFE_RANGE + 0x20,
    TEST_PENDING_TAP_ACTION = SAFE_RANGE + 0x21,
};

static uint16_t fake_time;

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

static active_key_state_t *test_default_slot(void) {
    return key_runtime_slot_for_position((keypos_t){.row = 0, .col = 0});
}

static active_key_state_t *test_other_slot(void) {
    return key_runtime_slot_for_position((keypos_t){.row = 0, .col = 1});
}

#define active_key (*test_default_slot())

static void test_reset_state(void) {
    noah_runtime_shared_state = (runtime_shared_state_t){0};
    fake_time                 = 0;
}

static handled_key_view_t test_resolve_handled_key(key_behavior_view_t behavior);

static key_runtime_slot_result_t test_step_handled_release(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, key_behavior_view_t behavior) {
    return key_runtime_slot_step(slot, (key_runtime_slot_event_t){
                                           .kind = KEY_RUNTIME_SLOT_EVENT_HANDLED_RELEASE,
                                           .data.handled_release =
                                               {
                                                   .keycode = keycode,
                                                   .key_pos = key_pos,
                                                   .key     = test_resolve_handled_key(behavior),
                                               },
                                       });
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
    (void)keycode;
    (void)tap_count;
    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    return keycode == TEST_MULTI_TAP_KEY && count < 3;
}

bool action_dispatch_is_layer_action(uint16_t action) {
    return IS_QK_MOMENTARY(action) || IS_QK_LAYER_TAP(action);
}

bool action_dispatch_is_layer_lock(uint16_t action) {
    (void)action;
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool is_layer_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

uint8_t behavior_get_layer(uint16_t keycode) {
    (void)keycode;
    return 0;
}

static handled_key_view_t test_resolve_handled_key(key_behavior_view_t behavior) {
    return (handled_key_view_t){
        .tap_action       = behavior.single.tap.action,
        .tap_repeat_count = behavior.single.tap.action == KC_NO ? 0 : 1,
        .hold             = behavior.single.hold,
        .long_hold        = behavior.single.long_hold,
        .hold_strategy    = KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT,
        .tap_hold_term    = behavior.tap_hold_term,
        .longer_hold_term = behavior.longer_hold_term,
        .multi_tap_term   = behavior.multi_tap_term,
        .layer            = behavior.is_momentary_layer ? behavior_get_layer(behavior.keycode) : UINT8_MAX,
        .pd_mode          = 0,
        .step_present     = behavior.single.tap.present || behavior.single.hold.present || behavior.single.long_hold.present,
        .has_more_taps    = behavior.has_multi_tap,
        .tap_resolves_on_press = false,
        .flags            = (behavior.has_multi_tap ? HANDLED_KEY_FLAG_MULTI_TAP : 0) | (behavior.is_momentary_layer ? HANDLED_KEY_FLAG_MOMENTARY_LAYER : 0) | (behavior.is_layer_tap ? HANDLED_KEY_FLAG_LAYER_TAP : 0),
    };
}

handled_key_view_t handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count) {
    (void)tap_count;
    key_behavior_view_t behavior = (key_behavior_view_t){
        .keycode       = keycode,
        .has_multi_tap = keycode == TEST_MULTI_TAP_KEY,
    };

    return test_resolve_handled_key(behavior);
}

hold_behavior_t handled_key_single_hold(handled_key_view_t key) {
    return key.hold;
}

hold_behavior_t handled_key_long_hold(handled_key_view_t key) {
    return key.long_hold;
}

uint16_t handled_key_tap_action(handled_key_view_t key) {
    return key.tap_action;
}

bool handled_key_uses_fallback_hold(handled_key_view_t key) {
    (void)key;
    return false;
}

bool handled_key_uses_implicit_hold(handled_key_view_t key) {
    (void)key;
    return false;
}

key_runtime_slot_hold_strategy_t handled_key_hold_strategy(handled_key_view_t key) {
    (void)key;
    return KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
}

uint16_t handled_key_tap_hold_term(handled_key_view_t key) {
    return key.tap_hold_term;
}

uint16_t handled_key_longer_hold_term(handled_key_view_t key) {
    return key.longer_hold_term;
}

uint16_t handled_key_multi_tap_term(handled_key_view_t key) {
    return key.multi_tap_term;
}

uint8_t handled_key_layer(handled_key_view_t key) {
    return key.layer;
}

pd_mode_mask_t handled_key_pd_mode(handled_key_view_t key) {
    (void)key;
    return 0;
}

bool handled_key_has_multi_tap(handled_key_view_t key) {
    return (key.flags & HANDLED_KEY_FLAG_MULTI_TAP) != 0;
}

bool handled_key_is_momentary_layer(handled_key_view_t key) {
    return (key.flags & HANDLED_KEY_FLAG_MOMENTARY_LAYER) != 0;
}

bool handled_key_is_layer_tap(handled_key_view_t key) {
    return (key.flags & HANDLED_KEY_FLAG_LAYER_TAP) != 0;
}

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    (void)action;
    return NOAH_ACTION_HOLD_KIND_SHARED;
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    (void)mt;
    return (delayed_action_mods_t){0};
}

static void test_non_passthrough_held_action_flashes(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .owner.keycode                 = KC_RIGHT_ALT,
        .lifecycle.held_action_keycode = SAFE_RANGE + 1,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_hold_active(flags));
    CHECK(key_feedback_flags_level_flash(flags));
}

static void test_repeat_hold_flashes_while_active(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .owner.keycode                   = KC_RIGHT_ALT,
        .lifecycle.repeat_binding_active = true,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_hold_active(flags));
    CHECK(key_feedback_flags_level_flash(flags));
}

static void test_fallback_hold_has_no_hold_feedback(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .owner.keycode                 = KC_RIGHT_ALT,
        .lifecycle.held_action_keycode = KC_RIGHT_ALT,
        .lifecycle.hold_strategy       = KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(flags == 0);
}

static void test_momentary_hold_preview_layer_is_exposed_before_threshold(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .owner.keycode = KC_RIGHT_ALT,
        .binding.hold =
            {
                .present = true,
                .action  = MO(3),
                .mode    = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE,
            },
    };

    CHECK(key_feedback_preview_layer() == 3);
}

static void test_cached_preview_layer_metadata_is_used_when_present(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .owner.keycode = KC_RIGHT_ALT,
        .semantic =
            {
                .valid         = true,
                .preview_layer = 4,
            },
    };

    CHECK(key_feedback_preview_layer() == 4);
}

static void test_momentary_hold_preview_layer_clears_once_layer_is_active(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .owner.keycode                 = KC_RIGHT_ALT,
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
        .lifecycle.held_action_keycode = MO(4),
    };

    CHECK(key_feedback_preview_layer() == UINT8_MAX);
}

static void test_non_layer_held_action_has_no_preview_layer(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .owner.keycode                 = KC_RIGHT_ALT,
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
        .lifecycle.held_action_keycode = SAFE_RANGE + 1,
    };

    CHECK(key_feedback_preview_layer() == UINT8_MAX);
}

static void test_feedback_falls_back_to_secondary_active_slot(void) {
    test_reset_state();

    *test_other_slot() = (active_key_state_t){
        .owner.keycode                 = KC_RIGHT_ALT,
        .lifecycle.held_action_keycode = SAFE_RANGE + 1,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_hold_active(flags));
    CHECK(key_feedback_flags_level_flash(flags));
}

static void test_multi_tap_pending_flag_uses_secondary_slot(void) {
    test_reset_state();

    test_other_slot()->pending_multi_tap = (multi_tap_t){
        .keycode = KC_RIGHT_ALT,
        .key_pos = (keypos_t){.row = 4, .col = 2},
        .count   = 1,
    };

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_multi_tap_pending(flags));
}

static void test_multi_tap_pending_flag_survives_quick_release_for_higher_taps(void) {
    key_runtime_slot_result_t release;
    keypos_t                  pos = {.row = 5, .col = 1};

    test_reset_state();

    active_key = (active_key_state_t){
        .owner.keycode           = TEST_MULTI_TAP_KEY,
        .owner.key_pos           = pos,
        .timing.tap_hold_term    = 120,
        .timing.longer_hold_term = 240,
        .pending_multi_tap =
            {
                .keycode          = TEST_MULTI_TAP_KEY,
                .key_pos          = pos,
                .timer            = (uint16_t)(fake_time - 50),
                .count            = 2,
                .pending_hold     = true,
                .tap_action       = TEST_PENDING_TAP_ACTION,
                .tap_repeat_count = 1,
                .has_more_taps    = true,
                .tap_hold_term    = 120,
            },
    };

    release = test_step_handled_release(test_default_slot(), TEST_MULTI_TAP_KEY, pos, (key_behavior_view_t){.keycode = TEST_MULTI_TAP_KEY});

    CHECK(release.handled);
    CHECK(release.count == 0);

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_multi_tap_pending(flags));
}

static void test_secondary_hold_pending_survives_primary_layer_hold(void) {
    test_reset_state();

    active_key = (active_key_state_t){
        .owner.keycode                 = MO(2),
        .lifecycle.held_action_keycode = MO(2),
        .lifecycle.phase               = KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
    };

    *test_other_slot() = (active_key_state_t){
        .timer                   = (uint16_t)(fake_time - 150),
        .owner.keycode           = KC_LEFT,
        .timing.tap_hold_term    = 100,
        .timing.longer_hold_term = 300,
        .binding.hold            = TAP_ON_RELEASE_AFTER_HOLD(TEST_PENDING_TAP_ACTION),
        .binding.long_hold       = TAP_AT_HOLD_THRESHOLD(TEST_MULTI_TAP_KEY),
    };

    uint8_t flags = key_feedback_pack();
    CHECK(key_feedback_flags_hold_pending(flags));
    CHECK(!key_feedback_flags_hold_active(flags));
    CHECK(!key_feedback_flags_long_hold_active(flags));
}

int main(void) {
    test_non_passthrough_held_action_flashes();
    test_repeat_hold_flashes_while_active();
    test_fallback_hold_has_no_hold_feedback();
    test_momentary_hold_preview_layer_is_exposed_before_threshold();
    test_cached_preview_layer_metadata_is_used_when_present();
    test_momentary_hold_preview_layer_clears_once_layer_is_active();
    test_non_layer_held_action_has_no_preview_layer();
    test_feedback_falls_back_to_secondary_active_slot();
    test_multi_tap_pending_flag_uses_secondary_slot();
    test_multi_tap_pending_flag_survives_quick_release_for_higher_taps();
    test_secondary_hold_pending_survives_primary_layer_hold();

    puts("key_runtime_feedback host tests passed");
    return 0;
}

#undef active_key
