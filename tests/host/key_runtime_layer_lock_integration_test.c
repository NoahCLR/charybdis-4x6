#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_runtime_integration_harness.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/lib/key/behavior/key_behavior_lookup.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/state/ownership/layer_ownership.h"
#include "users/noah/lib/state/diagnostics/runtime_debug.h"
#include "users/noah/lib/state/shared/runtime_reset.h"
#include "users/noah/noah_keymap_ids.h"
#include "users/noah/noah_runtime.h"

enum {
    TEST_MULTI_TAP_KEY       = SAFE_RANGE + 0x70,
    TEST_ALT_ACTION          = SAFE_RANGE + 0x71,
    TEST_FOREIGN_RELEASE_KEY = 0x0004u,
    TEST_TOGGLE_TAP_KEY      = SAFE_RANGE + 0x72,
    TEST_ONESHOT_HOLD_KEY    = SAFE_RANGE + 0x73,
    TEST_NUM_LAYER           = 1,
    TEST_OTHER_LAYER         = 2,
};

layer_state_t layer_state;
extern bool key_runtime_integration_output_ready;

static uint16_t fake_time;

// The one layer action a press-and-hold branch holds (see held_action_register).
static struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
} test_held_layer;

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

static layer_state_t test_layer_mask(uint8_t layer) {
    return (layer_state_t)1u << layer;
}

static bool test_layer_active(uint8_t layer) {
    return layer < LAYER_COUNT && (layer_state & test_layer_mask(layer)) != 0;
}

static bool test_layer_locked(uint8_t layer) {
    return layer < LAYER_COUNT && layer_ownership_is_locked(layer);
}

static void test_reset_state(void) {
    fake_time   = 1000;
    layer_state = 0;
    test_held_layer.active = false;
    noah_runtime_reset_for_test();
}

static void test_run_thumb_like_double_tap_hold_cycle_with_release_keycode(keypos_t key_pos, uint16_t release_keycode) {
    const key_runtime_integration_step_t steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_MULTI_TAP_KEY, key_pos.row, key_pos.col), KEY_RUNTIME_INTEGRATION_RELEASE(TEST_MULTI_TAP_KEY, key_pos.row, key_pos.col), KEY_RUNTIME_INTEGRATION_ADVANCE(40), KEY_RUNTIME_INTEGRATION_PRESS(TEST_MULTI_TAP_KEY, key_pos.row, key_pos.col), KEY_RUNTIME_INTEGRATION_ADVANCE(360), KEY_RUNTIME_INTEGRATION_SCAN(), KEY_RUNTIME_INTEGRATION_RELEASE(release_keycode, key_pos.row, key_pos.col),
    };

    key_runtime_integration_run(&fake_time, steps, ARRAY_SIZE(steps));
}

static void test_run_thumb_like_double_tap_hold_cycle_with_intermediate_scan(keypos_t key_pos, uint16_t release_keycode, uint16_t pre_threshold_scan_ms, uint16_t hold_ms) {
    const key_runtime_integration_step_t steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_MULTI_TAP_KEY, key_pos.row, key_pos.col), KEY_RUNTIME_INTEGRATION_RELEASE(TEST_MULTI_TAP_KEY, key_pos.row, key_pos.col), KEY_RUNTIME_INTEGRATION_ADVANCE(40), KEY_RUNTIME_INTEGRATION_PRESS(TEST_MULTI_TAP_KEY, key_pos.row, key_pos.col), KEY_RUNTIME_INTEGRATION_ADVANCE(pre_threshold_scan_ms), KEY_RUNTIME_INTEGRATION_SCAN(), KEY_RUNTIME_INTEGRATION_ADVANCE(hold_ms), KEY_RUNTIME_INTEGRATION_SCAN(), KEY_RUNTIME_INTEGRATION_RELEASE(release_keycode, key_pos.row, key_pos.col),
    };

    key_runtime_integration_run(&fake_time, steps, ARRAY_SIZE(steps));
}

static void test_run_thumb_like_double_tap_hold_cycle(keypos_t key_pos) {
    test_run_thumb_like_double_tap_hold_cycle_with_release_keycode(key_pos, TEST_MULTI_TAP_KEY);
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

uint32_t timer_read32(void) {
    return fake_time;
}

uint32_t timer_elapsed32(uint32_t last) {
    return (uint32_t)(fake_time - last);
}

bool is_keyboard_master(void) {
    return true;
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

void tap_code16(uint16_t keycode) {
    (void)keycode;
}

void register_code16(uint16_t keycode) {
    (void)keycode;
}

void unregister_code16(uint16_t keycode) {
    (void)keycode;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & test_layer_mask(layer)) != 0;
}

void layer_on(uint8_t layer) {
    CHECK(layer < LAYER_COUNT);
    layer_state |= test_layer_mask(layer);
}

void layer_off(uint8_t layer) {
    CHECK(layer < LAYER_COUNT);
    layer_state &= (layer_state_t)~test_layer_mask(layer);
}

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    if (keycode == TEST_MULTI_TAP_KEY) {
        return (key_behavior_view_t){
            .keycode            = keycode,
            .handled            = true,
            .has_multi_tap      = true,
            .authored_tap_depth = 2u,
            .tap_hold_term      = 150,
            .longer_hold_term   = 350,
            .multi_tap_term     = 120,
            .single =
                {
                    .tap  = TAP_SENDS(LOCK_LAYER(TEST_OTHER_LAYER)),
                    .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(TEST_OTHER_LAYER)),
                },
        };
    }

    if (keycode == TEST_TOGGLE_TAP_KEY) {
        return (key_behavior_view_t){
            .keycode       = keycode,
            .handled       = true,
            .tap_hold_term = 150,
            .single =
                {
                    .tap  = TAP_SENDS(TG(TEST_OTHER_LAYER)),
                    .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(TEST_NUM_LAYER)),
                },
        };
    }

    if (keycode == TEST_ONESHOT_HOLD_KEY) {
        return (key_behavior_view_t){
            .keycode       = keycode,
            .handled       = true,
            .tap_hold_term = 150,
            .single =
                {
                    .tap  = TAP_SENDS(OSL(TEST_OTHER_LAYER)),
                    .hold = PRESS_AND_HOLD_UNTIL_RELEASE(TT(TEST_NUM_LAYER)),
                },
        };
    }

    return (key_behavior_view_t){
        .keycode = keycode,
    };
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    if (keycode == TEST_MULTI_TAP_KEY && tap_count == 2) {
        return (key_behavior_step_t){
            .tap       = TAP_SENDS(TEST_ALT_ACTION),
            .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(TEST_NUM_LAYER)),
        };
    }

    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    return keycode == TEST_MULTI_TAP_KEY && count < 2;
}

bool key_behavior_future_tap_path_has_foreign_pd_mode(uint16_t keycode, uint8_t count, pd_mode_mask_t base_mode) {
    (void)keycode;
    (void)count;
    (void)base_mode;
    return false;
}

uint8_t key_behavior_validate_all(void) {
    return 0u;
}

bool noah_synthetic_record_active(void) {
    return false;
}

bool owned_keycode_register(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool owned_keycode_unregister(uint16_t keycode) {
    (void)keycode;
    return false;
}

void owned_keycode_track_physical_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

bool owned_keycode_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    (void)keycode;
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    (void)keycode;
}

bool noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    (void)keycode;
    (void)pressed;
    return false;
}

void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count) {
    (void)keycode;
    (void)pressed;
    (void)tap_count;
}

bool noah_qmk_contract_try_play_via_macro(uint16_t keycode) {
    (void)keycode;
    return false;
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

void pointer_layer_policy_sync_layer_lock_anchor(void) {}

void pointer_layer_policy_take_back_qmk_toggle(uint16_t keycode, const keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    (void)action;
    return NULL;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool pd_mode_toggle_lock_state_at(pd_mode_mask_t mode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_toggle_lock_state(mode);
}

bool pd_mode_set_lock_state(pd_mode_mask_t mode, bool locked) {
    (void)mode;
    (void)locked;
    return false;
}

bool pd_mode_set_lock_state_at(pd_mode_mask_t mode, bool locked, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_set_lock_state(mode, locked);
}

bool pd_mode_handle_keycode_press(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool pd_mode_handle_keycode_press_at(uint16_t keycode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_handle_keycode_press(keycode);
}

bool pd_mode_handle_keycode_release(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool pd_mode_handle_keycode_release_at(uint16_t keycode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_handle_keycode_release(keycode);
}

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

void split_runtime_sync(void) {}

void split_runtime_sync_request(void) {}

void keyboard_mod_ownership_track_report_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    (void)mods;
    return 0;
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    (void)action;
    (void)mods;
}

void dispatch_delayed_action_at(keypos_t key_pos, uint16_t action, delayed_action_mods_t mods) {
    (void)key_pos;
    dispatch_delayed_action(action, mods);
}

// A held layer action runs through the action lifecycle as the firmware's
// held-action owner does, so a press-and-hold branch really holds its layer.
void held_action_register(keypos_t key_pos, uint16_t action) {
    if (!noah_action_keycode_is_owned_momentary_layer(action)) return;
    test_held_layer.active  = true;
    test_held_layer.key_pos = key_pos;
    test_held_layer.action  = action;
    noah_action_press(key_pos, action);
}

bool held_action_release_owned_by_key(keypos_t key_pos) {
    if (!(test_held_layer.active && test_held_layer.key_pos.row == key_pos.row && test_held_layer.key_pos.col == key_pos.col)) return false;
    test_held_layer.active = false;
    noah_action_release(key_pos, test_held_layer.action);
    return true;
}

void held_action_unregister(keypos_t key_pos, uint16_t action) {
    (void)action;
    (void)held_action_release_owned_by_key(key_pos);
}

bool held_modifier_release_owned_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

bool held_repeat_release_owned_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

void held_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    (void)key_pos;
    (void)action;
    (void)repeat_hz;
}

void held_repeat_tick(void) {}

bool held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
    return false;
}

static void test_double_tap_hold_toggles_num_layer_lock_off_on_second_cycle(void) {
    keypos_t key_pos = test_keypos(4, 2);

    test_reset_state();

    test_run_thumb_like_double_tap_hold_cycle(key_pos);
    CHECK(test_layer_locked(TEST_NUM_LAYER));
    CHECK(test_layer_active(TEST_NUM_LAYER));
    CHECK(!test_layer_locked(TEST_OTHER_LAYER));
    CHECK(!test_layer_active(TEST_OTHER_LAYER));

    fake_time = (uint16_t)(fake_time + 40);
    test_run_thumb_like_double_tap_hold_cycle(key_pos);
    CHECK(!test_layer_locked(TEST_NUM_LAYER));
    CHECK(!test_layer_active(TEST_NUM_LAYER));
    CHECK(!test_layer_locked(TEST_OTHER_LAYER));
    CHECK(!test_layer_active(TEST_OTHER_LAYER));
}

static void test_thumb_cycle_release_still_clears_slot_when_layer_change_resolves_to_other_keycode(void) {
    keypos_t key_pos = test_keypos(4, 2);

    test_reset_state();

    test_run_thumb_like_double_tap_hold_cycle_with_release_keycode(key_pos, TEST_FOREIGN_RELEASE_KEY);
    CHECK(test_layer_locked(TEST_NUM_LAYER));
    CHECK(test_layer_active(TEST_NUM_LAYER));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);

    fake_time = (uint16_t)(fake_time + 40);
    test_run_thumb_like_double_tap_hold_cycle_with_release_keycode(key_pos, TEST_FOREIGN_RELEASE_KEY);
    CHECK(!test_layer_locked(TEST_NUM_LAYER));
    CHECK(!test_layer_active(TEST_NUM_LAYER));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
}

static void test_double_tap_hold_with_prethreshold_scan_toggles_num_layer_only_once(void) {
    keypos_t key_pos = test_keypos(4, 2);

    test_reset_state();

    test_run_thumb_like_double_tap_hold_cycle_with_intermediate_scan(key_pos, TEST_MULTI_TAP_KEY, 120, 240);
    CHECK(test_layer_locked(TEST_NUM_LAYER));
    CHECK(test_layer_active(TEST_NUM_LAYER));

    fake_time = (uint16_t)(fake_time + 40);
    test_run_thumb_like_double_tap_hold_cycle_with_intermediate_scan(key_pos, TEST_MULTI_TAP_KEY, 120, 240);
    CHECK(!test_layer_locked(TEST_NUM_LAYER));
    CHECK(!test_layer_active(TEST_NUM_LAYER));
}

// A plain key press and release, returning whether QMK would go on to
// process either event itself.
static bool test_plain_key_tap(uint16_t keycode, keypos_t key_pos) {
    bool keep_press   = key_runtime_integration_process_record(keycode, key_pos, true);
    bool keep_release = key_runtime_integration_process_record(keycode, key_pos, false);
    fake_time         = (uint16_t)(fake_time + 20);
    return keep_press || keep_release;
}

// TG() on a plain key is the same lock LOCK_LAYER() sets, so a momentary hold
// of that layer no longer turns it off on release, and QMK never sees it.
static void test_plain_toggle_key_is_a_layer_lock(void) {
    keypos_t toggle_pos = test_keypos(1, 1);
    keypos_t hold_pos   = test_keypos(1, 2);

    test_reset_state();

    CHECK(!test_plain_key_tap(TG(TEST_NUM_LAYER), toggle_pos));
    CHECK(test_layer_locked(TEST_NUM_LAYER));
    CHECK(test_layer_active(TEST_NUM_LAYER));

    key_runtime_integration_process_record(MO(TEST_NUM_LAYER), hold_pos, true);
    key_runtime_integration_process_record(MO(TEST_NUM_LAYER), hold_pos, false);
    CHECK(test_layer_locked(TEST_NUM_LAYER));
    CHECK(test_layer_active(TEST_NUM_LAYER));

    CHECK(!test_plain_key_tap(LOCK_LAYER(TEST_NUM_LAYER), toggle_pos));
    CHECK(!test_layer_locked(TEST_NUM_LAYER));
    CHECK(!test_layer_active(TEST_NUM_LAYER));
}

// TO() keeps only its target locked; TO(0) returns to the base layer.
static void test_plain_goto_key_locks_only_its_layer(void) {
    keypos_t key_pos = test_keypos(2, 3);

    test_reset_state();

    CHECK(!test_plain_key_tap(TG(TEST_NUM_LAYER), key_pos));
    CHECK(!test_plain_key_tap(TO(TEST_OTHER_LAYER), key_pos));
    CHECK(!test_layer_locked(TEST_NUM_LAYER));
    CHECK(!test_layer_active(TEST_NUM_LAYER));
    CHECK(test_layer_locked(TEST_OTHER_LAYER));
    CHECK(test_layer_active(TEST_OTHER_LAYER));

    CHECK(!test_plain_key_tap(TO(0), key_pos));
    CHECK(!test_layer_locked(TEST_OTHER_LAYER));
    CHECK(layer_state == 0);
}

// TG() authored as a behaviour tap toggles the same lock.
static void test_behavior_tap_toggles_layer_with_tg(void) {
    keypos_t key_pos = test_keypos(3, 0);

    test_reset_state();

    const key_runtime_integration_step_t steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_TOGGLE_TAP_KEY, key_pos.row, key_pos.col),
        KEY_RUNTIME_INTEGRATION_ADVANCE(30),
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_TOGGLE_TAP_KEY, key_pos.row, key_pos.col),
        KEY_RUNTIME_INTEGRATION_ADVANCE(200),
        KEY_RUNTIME_INTEGRATION_SCAN(),
    };

    key_runtime_integration_run(&fake_time, steps, ARRAY_SIZE(steps));
    CHECK(test_layer_locked(TEST_OTHER_LAYER));
    CHECK(test_layer_active(TEST_OTHER_LAYER));

    key_runtime_integration_run(&fake_time, steps, ARRAY_SIZE(steps));
    CHECK(!test_layer_locked(TEST_OTHER_LAYER));
    CHECK(!test_layer_active(TEST_OTHER_LAYER));
}

// OSL() as a behaviour tap arms the same one-shot a plain OSL() key does, and
// TT() as a press-and-hold branch holds its layer like MO() until release.
static void test_behavior_tap_arms_osl_and_hold_holds_tt(void) {
    keypos_t key_pos = test_keypos(3, 1);

    test_reset_state();

    const key_runtime_integration_step_t tap[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_ONESHOT_HOLD_KEY, key_pos.row, key_pos.col),
        KEY_RUNTIME_INTEGRATION_ADVANCE(30),
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_ONESHOT_HOLD_KEY, key_pos.row, key_pos.col),
        KEY_RUNTIME_INTEGRATION_ADVANCE(200),
        KEY_RUNTIME_INTEGRATION_SCAN(),
    };
    key_runtime_integration_run(&fake_time, tap, ARRAY_SIZE(tap));
    CHECK(layer_ownership_oneshot_layer() == TEST_OTHER_LAYER);
    CHECK(test_layer_active(TEST_OTHER_LAYER));
    CHECK(!test_layer_locked(TEST_OTHER_LAYER));
    CHECK(layer_ownership_oneshot_consume());
    CHECK(!test_layer_active(TEST_OTHER_LAYER));

    const key_runtime_integration_step_t press[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_ONESHOT_HOLD_KEY, key_pos.row, key_pos.col),
        KEY_RUNTIME_INTEGRATION_ADVANCE(200),
        KEY_RUNTIME_INTEGRATION_SCAN(),
    };
    key_runtime_integration_run(&fake_time, press, ARRAY_SIZE(press));
    CHECK(test_layer_active(TEST_NUM_LAYER));
    CHECK(!test_layer_locked(TEST_NUM_LAYER));

    const key_runtime_integration_step_t release[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_ONESHOT_HOLD_KEY, key_pos.row, key_pos.col),
        KEY_RUNTIME_INTEGRATION_ADVANCE(200),
        KEY_RUNTIME_INTEGRATION_SCAN(),
    };
    key_runtime_integration_run(&fake_time, release, ARRAY_SIZE(release));
    CHECK(!test_layer_active(TEST_NUM_LAYER));
    CHECK(!test_layer_locked(TEST_NUM_LAYER));
    CHECK(layer_ownership_oneshot_layer() == UINT8_MAX);
}

static void test_profile_output_fence_drops_whole_key_presses(void) {
    keyrecord_t press   = {.event = {.key = {.row = 0u, .col = 0u}, .pressed = true, .type = KEY_EVENT}};
    keyrecord_t release = press;
    keyrecord_t other   = {.event = {.key = {.row = 1u, .col = 2u}, .pressed = true, .type = KEY_EVENT}};

    release.event.pressed = false;
    test_reset_state();

    // A press made during the fence loses its release too, even when the
    // fence lifts while the key is still held.
    key_runtime_integration_output_ready = false;
    CHECK(!noah_pre_process_record_user(KC_A, &press));
    key_runtime_integration_output_ready = true;
    CHECK(!noah_pre_process_record_user(KC_A, &release));

    // A press made before the fence keeps its release after the fence lifts.
    CHECK(noah_pre_process_record_user(KC_B, &other));
    key_runtime_integration_output_ready = false;
    other.event.pressed = true;
    CHECK(!noah_pre_process_record_user(KC_A, &press));
    CHECK(!noah_pre_process_record_user(KC_A, &release));
    key_runtime_integration_output_ready = true;
    other.event.pressed = false;
    CHECK(noah_pre_process_record_user(KC_B, &other));

    // Once paired, the key is ordinary again.
    CHECK(noah_pre_process_record_user(KC_A, &press));
    CHECK(noah_pre_process_record_user(KC_A, &release));
}

int main(void) {
    test_profile_output_fence_drops_whole_key_presses();
    test_double_tap_hold_toggles_num_layer_lock_off_on_second_cycle();
    test_thumb_cycle_release_still_clears_slot_when_layer_change_resolves_to_other_keycode();
    test_double_tap_hold_with_prethreshold_scan_toggles_num_layer_only_once();

    test_plain_toggle_key_is_a_layer_lock();
    test_plain_goto_key_locks_only_its_layer();
    test_behavior_tap_toggles_layer_with_tg();
    test_behavior_tap_arms_osl_and_hold_holds_tt();

    puts("key_runtime layer-lock integration tests passed");
    return 0;
}
