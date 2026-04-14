#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/runtime/key_runtime_feedback.h"
#include "users/noah/lib/key/runtime/key_runtime_index_internal.h"
#include "users/noah/lib/key/runtime/slot/key_runtime_slot_step.h"
#include "users/noah/lib/key/runtime/key_runtime_internal.h"
#include "host_handled_key_fixture.h"
#include "host_runtime_reset_fixture.h"

enum {
    TEST_MULTI_TAP_KEY      = SAFE_RANGE + 0x20,
    TEST_PENDING_TAP_ACTION = SAFE_RANGE + 0x21,
};

static uint16_t fake_time;
layer_state_t    layer_state;

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
    host_runtime_fixture_reset_userspace_runtime();
    fake_time = 0;
}

static void test_track_slot(active_key_state_t *slot, uint16_t keycode, keypos_t key_pos, key_runtime_slot_interaction_t interaction, key_runtime_slot_phase_t phase) {
    key_runtime_slot_track(slot, keycode, key_pos, interaction, phase);
}

static uint8_t test_feedback_pack(void) {
    return key_feedback_pack();
}

static uint8_t test_feedback_preview_layer(void) {
    return key_feedback_preview_layer();
}

static key_runtime_slot_interaction_t test_cached_interaction(handled_key_resolution_t key) {
    return host_key_runtime_slot_interaction_from_authored_resolution(key, handled_key_resolution_ctx_make((keypos_t){0}, (layer_state_t)1u << 0));
}

static handled_key_resolution_t test_resolve_handled_key(key_behavior_view_t behavior);

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

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    (void)layer_num;
    (void)row;
    (void)column;
    return KC_TRNS;
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

void clear_mods(void) {}
void clear_weak_mods(void) {}
void clear_oneshot_mods(void) {}
void clear_oneshot_locked_mods(void) {}
void send_keyboard_report(void) {}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    (void)keycode;
    (void)tap_count;
    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    return keycode == TEST_MULTI_TAP_KEY && count < 3;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

static handled_key_resolution_t test_resolve_handled_key(key_behavior_view_t behavior) {
    return (handled_key_resolution_t){
        .keycode          = behavior.keycode,
        .tap_count        = 1,
        .step             = behavior.single,
        .tap_hold_term    = behavior.tap_hold_term,
        .longer_hold_term = behavior.longer_hold_term,
        .multi_tap_term   = behavior.multi_tap_term,
        .layer            = behavior.is_momentary_layer ? behavior_get_layer(behavior.keycode) : UINT8_MAX,
        .pd_mode          = 0,
        .has_more_taps    = behavior.has_multi_tap,
        .flags            = (behavior.has_multi_tap ? HANDLED_KEY_FLAG_MULTI_TAP : 0) | (behavior.is_momentary_layer ? HANDLED_KEY_FLAG_MOMENTARY_LAYER : 0) | (behavior.is_layer_tap ? HANDLED_KEY_FLAG_LAYER_TAP : 0),
    };
}

handled_key_resolution_t handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count) {
    (void)tap_count;
    key_behavior_view_t behavior = (key_behavior_view_t){
        .keycode       = keycode,
        .has_multi_tap = keycode == TEST_MULTI_TAP_KEY,
    };

    return test_resolve_handled_key(behavior);
}

hold_behavior_t handled_key_resolution_hold(handled_key_resolution_t key) {
    return key.step.hold;
}

hold_behavior_t handled_key_resolution_long_hold(handled_key_resolution_t key) {
    return key.step.long_hold;
}

uint16_t handled_key_resolution_tap_action(handled_key_resolution_t key) {
    return key.step.tap.present ? key.step.tap.action : KC_NO;
}

bool handled_key_resolution_uses_fallback_hold(handled_key_resolution_t key) {
    return key.tap_count == 1 && key.keycode < SAFE_RANGE && !handled_key_resolution_is_momentary_layer(key) && key.step.tap.present && !key.step.hold.present && !key.step.long_hold.present;
}

bool handled_key_resolution_uses_implicit_hold(handled_key_resolution_t key) {
    (void)key;
    return false;
}

key_runtime_slot_hold_strategy_t handled_key_resolution_hold_strategy(handled_key_resolution_t key) {
    return handled_key_resolution_uses_fallback_hold(key) ? KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK : KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT;
}

uint8_t handled_key_resolution_tap_repeat_count(handled_key_resolution_t key) {
    return handled_key_resolution_tap_action(key) == KC_NO ? 0 : 1;
}

bool handled_key_resolution_tap_resolves_on_press(handled_key_resolution_t key) {
    (void)key;
    return false;
}

uint16_t handled_key_resolution_tap_hold_term(handled_key_resolution_t key) {
    return key.tap_hold_term;
}

uint16_t handled_key_resolution_longer_hold_term(handled_key_resolution_t key) {
    return key.longer_hold_term;
}

uint16_t handled_key_resolution_multi_tap_term(handled_key_resolution_t key) {
    return key.multi_tap_term;
}

uint8_t handled_key_resolution_layer(handled_key_resolution_t key) {
    return key.layer;
}

pd_mode_mask_t handled_key_resolution_pd_mode(handled_key_resolution_t key) {
    (void)key;
    return 0;
}

handled_key_resolution_ctx_t handled_key_resolution_ctx_live(keypos_t key_pos) {
    return handled_key_resolution_ctx_make(key_pos, (layer_state_t)1u << 0);
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

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    (void)mt;
    return (delayed_action_mods_t){0};
}

static void test_non_passthrough_held_action_flashes(void) {
    keypos_t pos = {.row = 0, .col = 0};

    test_reset_state();
    test_track_slot(test_default_slot(), KC_RIGHT_ALT, pos, key_runtime_slot_interaction_default(), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    key_runtime_slot_set_held_action_keycode(test_default_slot(), SAFE_RANGE + 1);

    uint8_t flags = test_feedback_pack();
    CHECK(key_feedback_flags_hold_active(flags));
    CHECK(key_feedback_flags_level_flash(flags));
}

static void test_repeat_hold_flashes_while_active(void) {
    keypos_t pos = {.row = 0, .col = 0};

    test_reset_state();
    test_track_slot(test_default_slot(), KC_RIGHT_ALT, pos, key_runtime_slot_interaction_default(), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    key_runtime_slot_set_repeat_binding_active(test_default_slot(), true);

    uint8_t flags = test_feedback_pack();
    CHECK(key_feedback_flags_hold_active(flags));
    CHECK(key_feedback_flags_level_flash(flags));
}

static void test_fallback_hold_has_no_hold_feedback(void) {
    keypos_t pos = {.row = 0, .col = 0};

    test_reset_state();
    test_track_slot(test_default_slot(), KC_RIGHT_ALT, pos,
                    test_cached_interaction((handled_key_resolution_t){
                        .keycode   = KC_RIGHT_ALT,
                        .tap_count = 1,
                        .step =
                            {
                                .tap = TAP_SENDS(KC_NO),
                            },
                        .layer         = UINT8_MAX,
                        .pd_mode       = 0,
                        .has_more_taps = false,
                        .flags         = HANDLED_KEY_FLAG_HANDLED,
                    }),
                    KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    key_runtime_slot_set_held_action_keycode(test_default_slot(), KC_RIGHT_ALT);

    uint8_t flags = test_feedback_pack();
    CHECK(flags == 0);
}

static void test_momentary_hold_preview_layer_is_exposed_before_threshold(void) {
    keypos_t pos = {.row = 0, .col = 0};

    test_reset_state();
    test_track_slot(test_default_slot(), KC_RIGHT_ALT, pos,
                    test_cached_interaction((handled_key_resolution_t){
                        .keycode   = KC_RIGHT_ALT,
                        .tap_count = 1,
                        .step =
                            {
                                .hold =
                                    {
                                        .present = true,
                                        .action  = MO(3),
                                        .mode    = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE,
                                    },
                            },
                        .tap_hold_term = 120,
                        .layer         = UINT8_MAX,
                        .pd_mode       = 0,
                        .has_more_taps = false,
                        .flags         = HANDLED_KEY_FLAG_HANDLED,
                    }),
                    KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);

    CHECK(test_feedback_preview_layer() == 3);
}

static void test_cached_preview_layer_metadata_is_used_when_present(void) {
    keypos_t pos = {.row = 0, .col = 0};

    test_reset_state();
    test_track_slot(test_default_slot(), KC_RIGHT_ALT, pos,
                    test_cached_interaction((handled_key_resolution_t){
                        .keycode   = KC_RIGHT_ALT,
                        .tap_count = 1,
                        .step =
                            {
                                .hold =
                                    {
                                        .present = true,
                                        .action  = MO(4),
                                        .mode    = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE,
                                    },
                            },
                        .tap_hold_term = 120,
                        .layer         = UINT8_MAX,
                        .pd_mode       = 0,
                        .has_more_taps = false,
                        .flags         = HANDLED_KEY_FLAG_HANDLED,
                    }),
                    KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);

    CHECK(test_feedback_preview_layer() == 4);
}

static void test_momentary_hold_preview_layer_stays_quiet_after_threshold_until_activation(void) {
    keypos_t pos = {.row = 0, .col = 0};

    test_reset_state();
    fake_time = 150;
    test_track_slot(test_default_slot(), KC_RIGHT_ALT, pos,
                    test_cached_interaction((handled_key_resolution_t){
                        .keycode   = KC_RIGHT_ALT,
                        .tap_count = 1,
                        .step =
                            {
                                .hold =
                                    {
                                        .present = true,
                                        .action  = MO(4),
                                        .mode    = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE,
                                    },
                            },
                        .tap_hold_term = 120,
                        .layer         = UINT8_MAX,
                        .pd_mode       = 0,
                        .has_more_taps = false,
                        .flags         = HANDLED_KEY_FLAG_HANDLED,
                    }),
                    KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);

    CHECK(test_feedback_preview_layer() == 4);
    CHECK(test_feedback_pack() == 0);
}

static void test_momentary_hold_preview_layer_clears_once_layer_is_active(void) {
    keypos_t pos = {.row = 0, .col = 0};

    test_reset_state();
    test_track_slot(test_default_slot(), KC_RIGHT_ALT, pos, key_runtime_slot_interaction_default(), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    key_runtime_slot_set_held_action_keycode(test_default_slot(), MO(4));
    key_runtime_slot_commit_hold_phase(test_default_slot(), true);

    CHECK(test_feedback_preview_layer() == UINT8_MAX);
}

static void test_non_layer_held_action_has_no_preview_layer(void) {
    keypos_t pos = {.row = 0, .col = 0};

    test_reset_state();
    test_track_slot(test_default_slot(), KC_RIGHT_ALT, pos, key_runtime_slot_interaction_default(), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    key_runtime_slot_set_held_action_keycode(test_default_slot(), SAFE_RANGE + 1);
    key_runtime_slot_commit_hold_phase(test_default_slot(), true);

    CHECK(test_feedback_preview_layer() == UINT8_MAX);
}

static void test_feedback_falls_back_to_secondary_active_slot(void) {
    keypos_t other_pos = {.row = 0, .col = 1};

    test_reset_state();
    test_track_slot(test_other_slot(), KC_RIGHT_ALT, other_pos, key_runtime_slot_interaction_default(), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    key_runtime_slot_set_held_action_keycode(test_other_slot(), SAFE_RANGE + 1);

    uint8_t flags = test_feedback_pack();
    CHECK(key_feedback_flags_hold_active(flags));
    CHECK(key_feedback_flags_level_flash(flags));
}

static void test_multi_tap_pending_flag_uses_secondary_slot(void) {
    keypos_t pos = {.row = 4, .col = 2};

    test_reset_state();
    key_runtime_slot_begin_pending_multi_tap(test_other_slot(), KC_RIGHT_ALT, pos, KC_NO, 0, 120, 180, false);

    uint8_t flags = test_feedback_pack();
    CHECK(key_feedback_flags_multi_tap_pending(flags));
}

static void test_multi_tap_pending_flag_survives_quick_release_for_higher_taps(void) {
    key_runtime_slot_result_t release;
    keypos_t                  pos = {.row = 5, .col = 1};

    test_reset_state();

    test_track_slot(test_default_slot(), TEST_MULTI_TAP_KEY, pos,
                    test_cached_interaction((handled_key_resolution_t){
                        .keycode   = TEST_MULTI_TAP_KEY,
                        .tap_count = 1,
                        .step =
                            {
                                .tap = TAP_SENDS(TEST_PENDING_TAP_ACTION),
                            },
                        .tap_hold_term    = 120,
                        .longer_hold_term = 240,
                        .has_more_taps    = false,
                        .flags            = HANDLED_KEY_FLAG_HANDLED | HANDLED_KEY_FLAG_MULTI_TAP,
                    }),
                    KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    key_runtime_slot_begin_pending_multi_tap(test_default_slot(), TEST_MULTI_TAP_KEY, pos, TEST_PENDING_TAP_ACTION, 1, 120, 180, true);
    test_default_slot()->pending_multi_tap.timer        = (uint16_t)(fake_time - 50);
    test_default_slot()->pending_multi_tap.count        = 2;
    test_default_slot()->pending_multi_tap.pending_hold = true;

    release = test_step_handled_release(test_default_slot(), TEST_MULTI_TAP_KEY, pos, (key_behavior_view_t){.keycode = TEST_MULTI_TAP_KEY});

    CHECK(release.handled);
    CHECK(release.count == 0);

    uint8_t flags = test_feedback_pack();
    CHECK(key_feedback_flags_multi_tap_pending(flags));
}

static void test_secondary_hold_pending_survives_primary_layer_hold(void) {
    keypos_t primary_pos = {.row = 0, .col = 0};
    keypos_t other_pos   = {.row = 0, .col = 1};

    test_reset_state();
    test_track_slot(test_default_slot(), MO(2), primary_pos, key_runtime_slot_interaction_default(), KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    key_runtime_slot_set_held_action_keycode(test_default_slot(), MO(2));
    key_runtime_slot_commit_hold_phase(test_default_slot(), true);

    test_track_slot(test_other_slot(), KC_LEFT, other_pos,
                    test_cached_interaction((handled_key_resolution_t){
                        .keycode   = KC_LEFT,
                        .tap_count = 1,
                        .step =
                            {
                                .hold      = TAP_ON_RELEASE_AFTER_HOLD(TEST_PENDING_TAP_ACTION),
                                .long_hold = TAP_AT_HOLD_THRESHOLD(TEST_MULTI_TAP_KEY),
                            },
                        .tap_hold_term    = 100,
                        .longer_hold_term = 300,
                        .has_more_taps    = false,
                        .flags            = HANDLED_KEY_FLAG_HANDLED,
                    }),
                    KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    test_other_slot()->timer = (uint16_t)(fake_time - 150);

    uint8_t flags = test_feedback_pack();
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
    test_momentary_hold_preview_layer_stays_quiet_after_threshold_until_activation();
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
