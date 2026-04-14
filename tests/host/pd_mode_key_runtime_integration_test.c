#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "key_runtime_integration_harness.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/lib/key/interaction/key_behavior.h"
#include "users/noah/lib/key/interaction/key_behavior_lookup.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"
#include "users/noah/lib/state/runtime/runtime_reset.h"
#include "users/noah/noah_runtime.h"

#ifndef KC_J
#    define KC_J 0x000Du
#endif

enum {
    TEST_PD_HOLD_KEY       = NOAH_KEYMAP_SAFE_RANGE + 0x10,
    TEST_PD_TAP_HOLD_TERM  = 120,
    TEST_PD_MULTI_TAP_TERM = 150,
    TEST_LAYER_BASE        = 0,
    TEST_LAYER_POINTER     = 1,
    TEST_LAYER_SYM         = 2,
};

layer_state_t    layer_state = 0;
static uint16_t  test_keymap[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
const key_behavior_t key_behaviors[] = {
    {
        .keycode        = TEST_PD_HOLD_KEY,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts[0] =
            {
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(VOLUME_MODE),
            },
    },
    {
        .keycode        = VOLUME_MODE,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts[1] =
            {
                .tap  = TAP_SENDS(LOCK_PD_MODE(VOLUME_MODE)),
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(BRIGHTNESS_MODE),
            },
    },
    {
        .keycode        = PINCH_MODE,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(KC_TRNS)},
                [1] = {.tap = TAP_SENDS(VIA_MACRO_6), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(ZOOM_MODE)},
            },
    },
};
const uint8_t key_behavior_count = ARRAY_SIZE(key_behaviors);

static uint16_t fake_time;
static uint16_t current_cpi;
static uint16_t default_dpi;
static uint8_t  split_sync_count;
static uint8_t  reset_volume_count;
static uint8_t  fake_mods;
static uint8_t  fake_weak_mods;
static uint8_t  fake_oneshot_mods;
static uint8_t  fake_oneshot_locked_mods;
static uint8_t  fake_managed_mods;
static uint8_t  fake_physical_mods;
static uint8_t  delayed_action_count;
static uint16_t last_delayed_action;
static delayed_action_mods_t last_delayed_mods;

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

static void test_reset_keymap(void) {
    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                test_keymap[layer][row][col] = KC_TRNS;
            }
        }
    }
}

static void test_set_keymap_key(uint8_t layer, keypos_t key_pos, uint16_t keycode) {
    test_keymap[layer][key_pos.row][key_pos.col] = keycode;
}

static void test_configure_pinch_transparent_profile_path(keypos_t key_pos) {
    test_set_keymap_key(TEST_LAYER_POINTER, key_pos, PINCH_MODE);
    test_set_keymap_key(TEST_LAYER_BASE, key_pos, LT(TEST_LAYER_SYM, KC_J));
    layer_state = test_layer_mask(TEST_LAYER_BASE) | test_layer_mask(TEST_LAYER_POINTER);
}

static void test_reset_state(void) {
    noah_runtime_reset_for_test();
    test_reset_keymap();

    fake_time               = 1000;
    current_cpi             = 0;
    default_dpi             = 900;
    split_sync_count        = 0;
    reset_volume_count      = 0;
    layer_state             = test_layer_mask(TEST_LAYER_BASE);
    fake_mods                = 0;
    fake_weak_mods           = 0;
    fake_oneshot_mods        = 0;
    fake_oneshot_locked_mods = 0;
    fake_managed_mods        = 0;
    fake_physical_mods       = 0;
    delayed_action_count     = 0;
    last_delayed_action      = KC_NO;
    last_delayed_mods        = (delayed_action_mods_t){0};
}

static keyrecord_t test_record(keypos_t key_pos, bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .key     = key_pos,
                .pressed = pressed,
            },
    };
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

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return test_keymap[layer_num][row][column];
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

void send_keyboard_report(void) {}

void tap_code16(uint16_t keycode) {
    (void)keycode;
}

void register_code(uint8_t keycode) {
    (void)keycode;
}

void unregister_code(uint8_t keycode) {
    (void)keycode;
}

void register_code16(uint16_t keycode) {
    (void)keycode;
}

void unregister_code16(uint16_t keycode) {
    (void)keycode;
}

void wait_ms(uint16_t ms) {
    (void)ms;
}

void action_dispatch(uint16_t action) {
    const pd_mode_def_t *lock_mode = pd_mode_lock_action_lookup(action);
    pd_mode_mask_t       mode      = pd_mode_for_keycode(action);

    if (lock_mode) {
        pd_mode_toggle_lock_state(lock_mode->mode_flag);
        return;
    }

    if (mode != 0) {
        pd_mode_handle_keycode_press(action);
        pd_mode_handle_keycode_release(action);
    }
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    (void)policy;
    action_dispatch(action);
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

bool macro_dispatch(uint16_t action) {
    (void)action;
    return false;
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

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    uint8_t mask = 0;

    if (!record) {
        return;
    }

    switch (keycode) {
        case KC_LEFT_GUI:
            mask = MOD_BIT(KC_LEFT_GUI);
            break;
        default:
            return;
    }

    if (record->event.pressed) {
        fake_physical_mods |= mask;
    } else {
        fake_physical_mods &= (uint8_t)~mask;
    }

    fake_mods = (uint8_t)(fake_mods | fake_physical_mods);
    if ((fake_physical_mods & mask) == 0 && (fake_managed_mods & mask) == 0) {
        fake_mods &= (uint8_t)~mask;
    }
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void keyboard_mod_ownership_register_mods(uint8_t mods) {
    fake_managed_mods |= mods;
    fake_mods |= mods;
}

void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    fake_managed_mods &= (uint8_t)~mods;
    fake_mods = (uint8_t)((fake_mods & (uint8_t)~mods) | fake_physical_mods | fake_managed_mods);
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    if (keycode == KC_LEFT_GUI) {
        keyboard_mod_ownership_register_mods(MOD_BIT(KC_LEFT_GUI));
    }
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    if (keycode == KC_LEFT_GUI) {
        keyboard_mod_ownership_unregister_mods(MOD_BIT(KC_LEFT_GUI));
    }
}

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    return (uint8_t)(mods & fake_managed_mods & (uint8_t)~fake_physical_mods);
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    (void)key_pos;
    (void)layer;
}

bool layer_ownership_toggle_lock_state(uint8_t layer) {
    (void)layer;
    return true;
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    (void)key_pos;
    return true;
}

keyboard_mod_state_t keyboard_mod_state_suspend(void) {
    keyboard_mod_state_t saved = {
        .real           = fake_mods,
        .weak           = fake_weak_mods,
        .oneshot        = fake_oneshot_mods,
        .oneshot_locked = fake_oneshot_locked_mods,
    };

    fake_mods                = 0;
    fake_weak_mods           = 0;
    fake_oneshot_mods        = 0;
    fake_oneshot_locked_mods = 0;
    return saved;
}

void keyboard_mod_state_apply(keyboard_mod_state_t state) {
    fake_mods                = state.real;
    fake_weak_mods           = state.weak;
    fake_oneshot_mods        = state.oneshot;
    fake_oneshot_locked_mods = state.oneshot_locked;
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
    delayed_action_count++;
    last_delayed_action = action;
    last_delayed_mods   = mods;
    action_dispatch(action);
}

void key_feedback_pulse_arm(bool long_hold_level) {
    (void)long_hold_level;
}

void split_runtime_sync(void) {
    split_sync_count++;
}

bool charybdis_get_pointer_dragscroll_enabled(void) {
    return false;
}

bool charybdis_get_pointer_sniping_enabled(void) {
    return false;
}

uint16_t charybdis_get_pointer_default_dpi(void) {
    return default_dpi;
}

void charybdis_set_pointer_dragscroll_enabled(bool enabled) {
    (void)enabled;
}

void charybdis_set_pointer_sniping_enabled(bool enabled) {
    (void)enabled;
}

void pointing_device_set_cpi(uint16_t cpi) {
    current_cpi = cpi;
}

report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_dragscroll_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_zoom_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_arrow_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

bool handle_arrow_mode_key(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void reset_volume_mode(void) {
    reset_volume_count++;
}

void reset_dragscroll_mode(void) {}

void reset_brightness_mode(void) {}
void reset_zoom_mode(void) {}
void reset_arrow_mode(void) {}

static void test_authored_single_press_preserves_default_pd_mode_hold(void) {
    keypos_t                             key_pos       = test_keypos(1, 2);
    const key_runtime_integration_step_t press_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(VOLUME_MODE).config != NULL);
    CHECK(pd_mode_for_keycode(VOLUME_MODE) == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active_snapshot() == 0);

    key_runtime_integration_run(&fake_time, press_steps, ARRAY_SIZE(press_steps));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == VOLUME_MODE);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == VOLUME_MODE);
    CHECK(split_sync_count >= 1);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(!pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    CHECK(reset_volume_count == 1);
    CHECK(current_cpi == default_dpi);
}

static void test_authored_hold_action_activates_pd_mode_while_held(void) {
    keypos_t                             key_pos    = test_keypos(1, 3);
    const key_runtime_integration_step_t scenario[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_PD_HOLD_KEY, 1, 3),
        KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_TAP_HOLD_TERM + 1),
        KEY_RUNTIME_INTEGRATION_SCAN(),
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_PD_HOLD_KEY, 1, 3),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(TEST_PD_HOLD_KEY).config != NULL);
    CHECK(pd_mode_local_active_snapshot() == 0);

    key_runtime_integration_run(&fake_time, scenario, 3);
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == TEST_PD_HOLD_KEY);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == VOLUME_MODE);

    key_runtime_integration_run(&fake_time, &scenario[3], 1);
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    CHECK(reset_volume_count == 1);
    CHECK(current_cpi == default_dpi);
}

static void test_authored_double_tap_lock_locks_pd_mode(void) {
    const key_runtime_integration_step_t scenario[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2), KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2), KEY_RUNTIME_INTEGRATION_ADVANCE(20), KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2), KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
    };

    test_reset_state();

    key_runtime_integration_run(&fake_time, scenario, ARRAY_SIZE(scenario));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked(PD_MODE_VOLUME));
    CHECK(noah_runtime_debug_slot_owner_keycode(test_keypos(1, 2)) == KC_NO);
    CHECK(reset_volume_count == 1);
}

static void test_authored_second_press_hold_branches_into_other_pd_mode(void) {
    keypos_t                             key_pos    = test_keypos(1, 2);
    const key_runtime_integration_step_t scenario[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2), KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2), KEY_RUNTIME_INTEGRATION_ADVANCE(20), KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2), KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_TAP_HOLD_TERM + 1), KEY_RUNTIME_INTEGRATION_SCAN(), KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
    };

    test_reset_state();

    key_runtime_integration_run(&fake_time, scenario, 6);
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_BRIGHTNESS);
    CHECK(pd_mode_local_active(PD_MODE_BRIGHTNESS));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(!pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == VOLUME_MODE);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == BRIGHTNESS_MODE);
    CHECK(reset_volume_count == 1);

    key_runtime_integration_run(&fake_time, &scenario[6], 1);
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    CHECK(current_cpi == default_dpi);
}

static void test_pinch_single_tap_masks_mode_owned_gui_from_delayed_replay(void) {
    keypos_t                             key_pos         = test_keypos(2, 4);
    const key_runtime_integration_step_t press_steps[]   = {
        KEY_RUNTIME_INTEGRATION_PRESS(PINCH_MODE, 2, 4),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_RELEASE(PINCH_MODE, 2, 4),
    };
    const key_runtime_integration_step_t flush_steps[] = {
        KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_MULTI_TAP_TERM + 1),
        KEY_RUNTIME_INTEGRATION_SCAN(),
    };

    test_reset_state();
    test_configure_pinch_transparent_profile_path(key_pos);
    fake_mods = MOD_BIT(KC_LEFT_SHIFT);

    CHECK(key_behavior_lookup(PINCH_MODE).config != NULL);
    CHECK(pd_mode_for_keycode(PINCH_MODE) == PD_MODE_PINCH);

    key_runtime_integration_run(&fake_time, press_steps, ARRAY_SIZE(press_steps));
    CHECK(pd_mode_local_active(PD_MODE_PINCH));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == PINCH_MODE);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == PINCH_MODE);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(!pd_mode_local_active(PD_MODE_PINCH));
    CHECK(fake_mods == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);

    key_runtime_integration_run(&fake_time, flush_steps, ARRAY_SIZE(flush_steps));
    CHECK(delayed_action_count == 1);
    CHECK(last_delayed_action == KC_J);
    CHECK(last_delayed_mods.real == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(last_delayed_mods.weak == 0);
    CHECK(last_delayed_mods.oneshot == 0);
    CHECK(last_delayed_mods.oneshot_locked == 0);
}

static void test_pinch_single_tap_preserves_physically_held_gui_on_delayed_replay(void) {
    keypos_t                             key_pos         = test_keypos(2, 4);
    const key_runtime_integration_step_t press_steps[]   = {
        KEY_RUNTIME_INTEGRATION_PRESS(PINCH_MODE, 2, 4),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_RELEASE(PINCH_MODE, 2, 4),
    };
    const key_runtime_integration_step_t flush_steps[] = {
        KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_MULTI_TAP_TERM + 1),
        KEY_RUNTIME_INTEGRATION_SCAN(),
    };
    keyrecord_t gui_press = {
         .event =
             {
                 .key     = {.row = 0, .col = 0},
                 .pressed = true,
             },
    };

    test_reset_state();
    test_configure_pinch_transparent_profile_path(key_pos);
    fake_mods = MOD_BIT(KC_LEFT_SHIFT);
    keyboard_mod_ownership_track_physical_keycode_event(KC_LEFT_GUI, &gui_press);

    key_runtime_integration_run(&fake_time, press_steps, ARRAY_SIZE(press_steps));
    CHECK(pd_mode_local_active(PD_MODE_PINCH));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == PINCH_MODE);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(!pd_mode_local_active(PD_MODE_PINCH));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    key_runtime_integration_run(&fake_time, flush_steps, ARRAY_SIZE(flush_steps));
    CHECK(delayed_action_count == 1);
    CHECK(last_delayed_action == KC_J);
    CHECK(last_delayed_mods.real == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));
    CHECK(last_delayed_mods.weak == 0);
    CHECK(last_delayed_mods.oneshot == 0);
    CHECK(last_delayed_mods.oneshot_locked == 0);
}

static void test_pinch_masks_mode_owned_gui_during_concurrent_plain_key_processing(void) {
    keypos_t pinch_key_pos = test_keypos(2, 4);
    keypos_t plain_key_pos = test_keypos(0, 1);
    keyrecord_t plain_press = test_record(plain_key_pos, true);

    test_reset_state();
    test_configure_pinch_transparent_profile_path(pinch_key_pos);
    fake_mods = MOD_BIT(KC_LEFT_SHIFT);

    CHECK(!key_runtime_integration_process_record(PINCH_MODE, pinch_key_pos, true));
    CHECK(pd_mode_local_active(PD_MODE_PINCH));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    CHECK(noah_pre_process_record_user(KC_C, &plain_press));
    CHECK(noah_process_record_user(KC_C, &plain_press));
    CHECK(fake_mods == MOD_BIT(KC_LEFT_SHIFT));

    noah_post_process_record_user(KC_C, &plain_press);
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));
}

static void test_pinch_keeps_physically_held_gui_visible_during_concurrent_plain_key_processing(void) {
    keypos_t pinch_key_pos = test_keypos(2, 4);
    keypos_t gui_key_pos   = test_keypos(0, 0);
    keypos_t plain_key_pos = test_keypos(0, 1);
    keyrecord_t gui_press   = test_record(gui_key_pos, true);
    keyrecord_t plain_press = test_record(plain_key_pos, true);

    test_reset_state();
    test_configure_pinch_transparent_profile_path(pinch_key_pos);
    fake_mods = MOD_BIT(KC_LEFT_SHIFT);

    CHECK(!key_runtime_integration_process_record(PINCH_MODE, pinch_key_pos, true));
    CHECK(pd_mode_local_active(PD_MODE_PINCH));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    CHECK(noah_pre_process_record_user(KC_LEFT_GUI, &gui_press));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    CHECK(noah_pre_process_record_user(KC_C, &plain_press));
    CHECK(noah_process_record_user(KC_C, &plain_press));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    noah_post_process_record_user(KC_C, &plain_press);
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));
}

int main(void) {
    test_authored_single_press_preserves_default_pd_mode_hold();
    test_authored_hold_action_activates_pd_mode_while_held();
    test_authored_double_tap_lock_locks_pd_mode();
    test_authored_second_press_hold_branches_into_other_pd_mode();
    test_pinch_single_tap_masks_mode_owned_gui_from_delayed_replay();
    test_pinch_single_tap_preserves_physically_held_gui_on_delayed_replay();
    test_pinch_masks_mode_owned_gui_during_concurrent_plain_key_processing();
    test_pinch_keeps_physically_held_gui_visible_during_concurrent_plain_key_processing();

    puts("pd_mode_key_runtime_integration host tests passed");
    return 0;
}
