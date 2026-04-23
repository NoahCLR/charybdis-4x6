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
#include "users/noah/lib/key/runtime/core/runtime.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"
#include "users/noah/lib/state/runtime/runtime_reset.h"
#include "users/noah/noah_runtime.h"

#ifndef KC_J
#    define KC_J 0x000Du
#endif
#ifndef KC_SLSH
#    define KC_SLSH 0x0038u
#endif
#ifndef KC_DOT
#    define KC_DOT 0x0037u
#endif

enum {
    TEST_PD_HOLD_KEY       = NOAH_KEYMAP_SAFE_RANGE + 0x10,
    TEST_HANDLED_TAP_KEY   = NOAH_KEYMAP_SAFE_RANGE + 0x11,
    TEST_LAYER_HOLD_KEY    = NOAH_KEYMAP_SAFE_RANGE + 0x12,
    TEST_PD_TAP_HOLD_TERM  = 120,
    TEST_PD_MULTI_TAP_TERM = 150,
    TEST_LAYER_BASE        = 0,
    TEST_LAYER_POINTER     = 1,
    TEST_LAYER_SYM         = 2,
    TEST_LAYER_NAV         = 3,
};

#ifndef CHARYBDIS_AUTO_SNIPING_LAYER
#    define CHARYBDIS_AUTO_SNIPING_LAYER TEST_LAYER_NAV
#endif

#ifndef DPI_MOD
#    define DPI_MOD 0x7000u
#endif
#ifndef DPI_RMOD
#    define DPI_RMOD 0x7001u
#endif
#ifndef S_D_MOD
#    define S_D_MOD 0x7002u
#endif
#ifndef S_D_RMOD
#    define S_D_RMOD 0x7003u
#endif

layer_state_t        layer_state = 0;
static uint16_t      test_keymap[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
const key_behavior_t key_behaviors[] = {
    {
        .keycode        = KC_RIGHT_ALT,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts[0] =
            {
                .tap = TAP_SENDS(ARROW_MODE_LOCK),
            },
    },
    {
        .keycode        = KC_LEFT_GUI,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts[1] =
            {
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LEFT_ALT),
            },
    },
    {
        .keycode       = LT(TEST_LAYER_NAV, KC_SLSH),
        .tap_hold_term = 100,
        .tap_counts[1] =
            {
                .hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(TEST_LAYER_NAV)),
            },
    },
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
        .keycode        = TEST_HANDLED_TAP_KEY,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts[0] =
            {
                .tap = TAP_SENDS(KC_J),
            },
    },
    {
        .keycode        = TEST_LAYER_HOLD_KEY,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts =
            {
                [0] = {.tap = TAP_SENDS(LOCK_LAYER(TEST_LAYER_NAV)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(TEST_LAYER_NAV))},
                [1] = {.tap = TAP_SENDS(KC_C)},
            },
    },
    {
        .keycode        = VOLUME_MODE,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts[1] =
            {
                .tap  = TAP_SENDS(VOLUME_MODE_LOCK),
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

static uint16_t              fake_time;
static uint16_t              current_cpi;
static uint16_t              default_dpi;
static uint8_t               split_sync_count;
static uint8_t               reset_volume_count;
static uint8_t               layer_state_set_count;
static uint8_t               layer_off_count;
static uint8_t               fake_mods;
static uint8_t               fake_weak_mods;
static uint8_t               fake_oneshot_mods;
static uint8_t               fake_oneshot_locked_mods;
static uint8_t               fake_managed_mods;
static uint8_t               fake_physical_mods;
static uint8_t               delayed_action_count;
static uint8_t               auto_mouse_layer_off_count;
static uint8_t               auto_mouse_layer_target;
static bool                  auto_mouse_enabled;
static bool                  auto_mouse_toggled;
static int8_t                auto_mouse_key_tracker;
static bool                  dragscroll_enabled;
static bool                  sniping_enabled;
static uint8_t               auto_mouse_layer_off_active_slot_count;
static bool                  auto_mouse_layer_off_pending_fallback;
static uint8_t               auto_mouse_layer_off_real_mods;
static uint8_t               auto_mouse_layer_off_managed_mods;
static uint8_t               auto_mouse_layer_off_physical_mods;
static uint16_t              tap_code16_count;
static uint16_t              last_tap_code16;
static uint8_t               reset_dragscroll_count;
static uint16_t              last_delayed_action;
static delayed_action_mods_t last_delayed_mods;

typedef enum {
    TEST_RELEASE_TARGET_CHILD = 0,
    TEST_RELEASE_TARGET_PARENT,
    TEST_RELEASE_TARGET_GUI,
} test_release_target_t;

typedef struct {
    test_release_target_t order[3];
} test_release_order_case_t;

static const test_release_order_case_t test_release_orders[] = {
    {.order = {TEST_RELEASE_TARGET_CHILD, TEST_RELEASE_TARGET_PARENT, TEST_RELEASE_TARGET_GUI}}, {.order = {TEST_RELEASE_TARGET_CHILD, TEST_RELEASE_TARGET_GUI, TEST_RELEASE_TARGET_PARENT}}, {.order = {TEST_RELEASE_TARGET_PARENT, TEST_RELEASE_TARGET_CHILD, TEST_RELEASE_TARGET_GUI}}, {.order = {TEST_RELEASE_TARGET_PARENT, TEST_RELEASE_TARGET_GUI, TEST_RELEASE_TARGET_CHILD}}, {.order = {TEST_RELEASE_TARGET_GUI, TEST_RELEASE_TARGET_CHILD, TEST_RELEASE_TARGET_PARENT}}, {.order = {TEST_RELEASE_TARGET_GUI, TEST_RELEASE_TARGET_PARENT, TEST_RELEASE_TARGET_CHILD}},
};

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

    fake_time                              = 1000;
    current_cpi                            = 0;
    default_dpi                            = 900;
    split_sync_count                       = 0;
    reset_volume_count                     = 0;
    layer_state_set_count                  = 0;
    layer_off_count                        = 0;
    layer_state                            = test_layer_mask(TEST_LAYER_BASE);
    fake_mods                              = 0;
    fake_weak_mods                         = 0;
    fake_oneshot_mods                      = 0;
    fake_oneshot_locked_mods               = 0;
    fake_managed_mods                      = 0;
    fake_physical_mods                     = 0;
    delayed_action_count                   = 0;
    auto_mouse_layer_off_count             = 0;
    auto_mouse_layer_target                = TEST_LAYER_POINTER;
    auto_mouse_enabled                     = true;
    auto_mouse_toggled                     = false;
    auto_mouse_key_tracker                 = 0;
    dragscroll_enabled                     = false;
    sniping_enabled                        = false;
    auto_mouse_layer_off_active_slot_count = 0;
    auto_mouse_layer_off_pending_fallback  = false;
    auto_mouse_layer_off_real_mods         = 0;
    auto_mouse_layer_off_managed_mods      = 0;
    auto_mouse_layer_off_physical_mods     = 0;
    tap_code16_count                       = 0;
    last_tap_code16                        = KC_NO;
    reset_dragscroll_count                 = 0;
    last_delayed_action                    = KC_NO;
    last_delayed_mods                      = (delayed_action_mods_t){0};
}

static keyrecord_t test_record(keypos_t key_pos, bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .type    = KEY_EVENT,
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

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

static void test_apply_layer_state(layer_state_t next_state) {
    layer_state_set_count++;
    layer_state = noah_layer_state_set_user(next_state);
}

void layer_on(uint8_t layer) {
    test_apply_layer_state(layer_state | ((layer_state_t)1u << layer));
}

void layer_off(uint8_t layer) {
    layer_off_count++;
    test_apply_layer_state(layer_state & (layer_state_t) ~((layer_state_t)1u << layer));
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
    tap_code16_count++;
    last_tap_code16 = keycode;
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
        case KC_LEFT_ALT:
            mask = MOD_BIT(KC_LEFT_ALT);
            break;
        case KC_RIGHT_ALT:
            mask = MOD_BIT(KC_RIGHT_ALT);
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
    switch (keycode) {
        case KC_LEFT_GUI:
            keyboard_mod_ownership_register_mods(MOD_BIT(KC_LEFT_GUI));
            break;
        case KC_LEFT_ALT:
            keyboard_mod_ownership_register_mods(MOD_BIT(KC_LEFT_ALT));
            break;
        case KC_RIGHT_ALT:
            keyboard_mod_ownership_register_mods(MOD_BIT(KC_RIGHT_ALT));
            break;
        default:
            break;
    }
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    switch (keycode) {
        case KC_LEFT_GUI:
            keyboard_mod_ownership_unregister_mods(MOD_BIT(KC_LEFT_GUI));
            break;
        case KC_LEFT_ALT:
            keyboard_mod_ownership_unregister_mods(MOD_BIT(KC_LEFT_ALT));
            break;
        case KC_RIGHT_ALT:
            keyboard_mod_ownership_unregister_mods(MOD_BIT(KC_RIGHT_ALT));
            break;
        default:
            break;
    }
}

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    return (uint8_t)(mods & fake_managed_mods & (uint8_t)~fake_physical_mods);
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    (void)key_pos;
    (void)layer;
}

bool layer_ownership_is_locked(uint8_t layer) {
    (void)layer;
    return false;
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

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    delayed_action_count++;
    last_delayed_action = action;
    last_delayed_mods   = mods;
    action_dispatch(action);
}

void dispatch_delayed_action_at(keypos_t key_pos, uint16_t action, delayed_action_mods_t mods) {
    (void)key_pos;
    dispatch_delayed_action(action, mods);
}

void key_feedback_pulse_arm(bool long_hold_level) {
    (void)long_hold_level;
}

void split_runtime_sync(void) {
    split_sync_count++;
}

void split_runtime_sync_request(void) {
    split_runtime_sync();
}

bool get_auto_mouse_toggle(void) {
    return auto_mouse_toggled;
}

int8_t get_auto_mouse_key_tracker(void) {
    return auto_mouse_key_tracker;
}

uint8_t get_auto_mouse_layer(void) {
    return auto_mouse_layer_target;
}

uint16_t auto_mouse_get_time_elapsed(void) {
    return 0;
}

bool is_auto_mouse_active(void) {
    return false;
}

void set_auto_mouse_enable(bool enable) {
    auto_mouse_enabled = enable;
}

void set_auto_mouse_layer(uint8_t layer) {
    auto_mouse_layer_target = layer;
}

void auto_mouse_layer_off(void) {
    keypos_t pending_fallback_pos;

    auto_mouse_layer_off_count++;
    auto_mouse_layer_off_active_slot_count = noah_runtime_debug_active_slot_count();
    auto_mouse_layer_off_pending_fallback  = noah_runtime_debug_pending_fallback_slot_key_pos(&pending_fallback_pos);
    auto_mouse_layer_off_real_mods         = fake_mods;
    auto_mouse_layer_off_managed_mods      = fake_managed_mods;
    auto_mouse_layer_off_physical_mods     = fake_physical_mods;

    if (layer_state_cmp(layer_state, auto_mouse_layer_target) && auto_mouse_enabled && !auto_mouse_toggled && auto_mouse_key_tracker == 0) {
        layer_off(auto_mouse_layer_target);
    }
}

void auto_mouse_toggle(void) {
    auto_mouse_toggled = !auto_mouse_toggled;
}

void auto_mouse_keyevent(bool pressed) {
    auto_mouse_key_tracker += pressed ? 1 : -1;
}

bool charybdis_get_pointer_dragscroll_enabled(void) {
    return dragscroll_enabled;
}

bool charybdis_get_pointer_sniping_enabled(void) {
    return sniping_enabled;
}

uint16_t charybdis_get_pointer_default_dpi(void) {
    return default_dpi;
}

void charybdis_set_pointer_dragscroll_enabled(bool enabled) {
    dragscroll_enabled = enabled;
}

void charybdis_set_pointer_sniping_enabled(bool enabled) {
    sniping_enabled = enabled;
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

void reset_dragscroll_mode(void) {
    reset_dragscroll_count++;
}

void reset_brightness_mode(void) {}
void reset_zoom_mode(void) {}
void reset_arrow_mode(void) {}

static void test_activate_gui_double_tap_alt_hold(keypos_t gui_pos) {
    const key_runtime_integration_step_t initial_tap_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(KC_LEFT_GUI, 0, 0), KEY_RUNTIME_INTEGRATION_RELEASE(KC_LEFT_GUI, 0, 0), KEY_RUNTIME_INTEGRATION_ADVANCE(40), KEY_RUNTIME_INTEGRATION_PRESS(KC_LEFT_GUI, 0, 0), KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_TAP_HOLD_TERM + 1), KEY_RUNTIME_INTEGRATION_SCAN(),
    };

    CHECK(key_behavior_lookup(KC_LEFT_GUI).config != NULL);

    key_runtime_integration_run(&fake_time, initial_tap_steps, ARRAY_SIZE(initial_tap_steps));
    CHECK(noah_runtime_debug_slot_owner_keycode(gui_pos) == KC_LEFT_GUI);
    CHECK(noah_runtime_debug_slot_held_action_keycode(gui_pos) == KC_LEFT_ALT);
    CHECK((fake_mods & MOD_BIT(KC_LEFT_ALT)) != 0);
}

static void test_assert_gui_pd_overlap_quiescent(keypos_t gui_pos, keypos_t parent_pos) {
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(gui_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_owner_keycode(parent_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(gui_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(parent_pos) == KC_NO);
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK((fake_mods & (MOD_BIT(KC_LEFT_GUI) | MOD_BIT(KC_LEFT_ALT))) == 0);
}

static void test_assert_pd_overlap_quiescent(keypos_t parent_pos, keypos_t child_pos) {
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(parent_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(parent_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == KC_NO);
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(!dragscroll_enabled);
}

static void test_authored_single_press_preserves_default_pd_mode_hold(void) {
    keypos_t                             key_pos       = test_keypos(1, 2);
    const key_runtime_integration_step_t press_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
        KEY_RUNTIME_INTEGRATION_SCAN(),
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
    CHECK(split_sync_count == 0);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    const press_token_t *release_token = key_runtime_core_press_token_at(key_pos);

    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(!pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    CHECK(reset_volume_count == 1);
    CHECK(current_cpi == default_dpi);
}

static void test_authored_single_press_pd_mode_hold_dispatches_plain_taps_immediately(void) {
    keypos_t                             key_pos              = test_keypos(1, 2);
    const key_runtime_integration_step_t hold_and_tap_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2),
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_HANDLED_TAP_KEY, 1, 3),
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_HANDLED_TAP_KEY, 1, 3),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
    };

    test_reset_state();

    key_runtime_integration_run(&fake_time, hold_and_tap_steps, ARRAY_SIZE(hold_and_tap_steps));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == VOLUME_MODE);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == VOLUME_MODE);
    CHECK(tap_code16_count == 1);
    CHECK(last_tap_code16 == KC_J);
    CHECK(delayed_action_count == 0);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(tap_code16_count == 1);
    CHECK(last_tap_code16 == KC_J);
    CHECK(delayed_action_count == 0);
}

static void test_raw_lt_hold_dispatches_authored_tap_key_immediately(void) {
    keypos_t                             hold_pos     = test_keypos(1, 4);
    keypos_t                             child_pos    = test_keypos(3, 5);
    const uint16_t                       hold_key     = LT(TEST_LAYER_NAV, KC_SLSH);
    const key_runtime_integration_step_t hold_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(hold_key, 1, 4),
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_PRESS(KC_RIGHT_ALT, 3, 5),
        KEY_RUNTIME_INTEGRATION_RELEASE(KC_RIGHT_ALT, 3, 5),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(hold_key, 1, 4),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(KC_RIGHT_ALT).config != NULL);

    key_runtime_integration_run(&fake_time, hold_steps, ARRAY_SIZE(hold_steps));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
    CHECK(delayed_action_count == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(pd_mode_local_active(PD_MODE_ARROW));
    CHECK(pd_mode_local_locked(PD_MODE_ARROW));
}

static void test_raw_lt_hold_dispatches_authored_plain_tap_immediately(void) {
    const uint16_t                       hold_key     = LT(TEST_LAYER_NAV, KC_SLSH);
    const key_runtime_integration_step_t hold_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(hold_key, 1, 4),
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_HANDLED_TAP_KEY, 3, 5),
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_HANDLED_TAP_KEY, 3, 5),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(hold_key, 1, 4),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(TEST_HANDLED_TAP_KEY).config != NULL);

    key_runtime_integration_run(&fake_time, hold_steps, ARRAY_SIZE(hold_steps));
    CHECK(tap_code16_count == 1);
    CHECK(last_tap_code16 == KC_J);
    CHECK(delayed_action_count == 0);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(tap_code16_count == 1);
    CHECK(delayed_action_count == 0);
}

static void test_authored_layer_hold_dispatches_authored_tap_key_immediately(void) {
    keypos_t                             hold_pos     = test_keypos(1, 4);
    keypos_t                             child_pos    = test_keypos(3, 5);
    const key_runtime_integration_step_t hold_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_LAYER_HOLD_KEY, 1, 4), KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_TAP_HOLD_TERM + 1), KEY_RUNTIME_INTEGRATION_SCAN(), KEY_RUNTIME_INTEGRATION_PRESS(KC_RIGHT_ALT, 3, 5), KEY_RUNTIME_INTEGRATION_RELEASE(KC_RIGHT_ALT, 3, 5),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_LAYER_HOLD_KEY, 1, 4),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(TEST_LAYER_HOLD_KEY).config != NULL);
    CHECK(key_behavior_lookup(KC_RIGHT_ALT).config != NULL);

    key_runtime_integration_run(&fake_time, hold_steps, ARRAY_SIZE(hold_steps));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
    CHECK(delayed_action_count == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(pd_mode_local_active(PD_MODE_ARROW));
    CHECK(pd_mode_local_locked(PD_MODE_ARROW));
}

static void test_authored_layer_hold_dispatches_authored_plain_tap_immediately(void) {
    const key_runtime_integration_step_t hold_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_LAYER_HOLD_KEY, 1, 4), KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_TAP_HOLD_TERM + 1), KEY_RUNTIME_INTEGRATION_SCAN(), KEY_RUNTIME_INTEGRATION_PRESS(TEST_HANDLED_TAP_KEY, 3, 5), KEY_RUNTIME_INTEGRATION_RELEASE(TEST_HANDLED_TAP_KEY, 3, 5),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_LAYER_HOLD_KEY, 1, 4),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(TEST_LAYER_HOLD_KEY).config != NULL);
    CHECK(key_behavior_lookup(TEST_HANDLED_TAP_KEY).config != NULL);

    key_runtime_integration_run(&fake_time, hold_steps, ARRAY_SIZE(hold_steps));
    CHECK(tap_code16_count == 1);
    CHECK(last_tap_code16 == KC_J);
    CHECK(delayed_action_count == 0);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(tap_code16_count == 1);
    CHECK(delayed_action_count == 0);
}

static void test_interrupted_locked_pd_mode_press_still_toggles_lock_on_release(void) {
    keypos_t                             key_pos               = test_keypos(1, 2);
    const key_runtime_integration_step_t press_and_interrupt[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2),
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_PRESS(KC_J, 1, 3),
        KEY_RUNTIME_INTEGRATION_RELEASE(KC_J, 1, 3),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
    };

    test_reset_state();

    CHECK(pd_mode_set_lock_state(PD_MODE_VOLUME, true));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    CHECK(reset_volume_count == 0);

    key_runtime_integration_run(&fake_time, press_and_interrupt, ARRAY_SIZE(press_and_interrupt));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == VOLUME_MODE);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == VOLUME_MODE);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(!pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(!pd_mode_local_locked(PD_MODE_VOLUME));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    CHECK(reset_volume_count == 1);
}

static void test_authored_pd_mode_hold_dispatches_authored_tap_key_immediately(void) {
    keypos_t                             hold_pos     = test_keypos(1, 2);
    keypos_t                             child_pos    = test_keypos(3, 5);
    const key_runtime_integration_step_t hold_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2),
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_PRESS(KC_RIGHT_ALT, 3, 5),
        KEY_RUNTIME_INTEGRATION_RELEASE(KC_RIGHT_ALT, 3, 5),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(VOLUME_MODE).config != NULL);
    CHECK(key_behavior_lookup(KC_RIGHT_ALT).config != NULL);

    key_runtime_integration_run(&fake_time, hold_steps, ARRAY_SIZE(hold_steps));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
    CHECK(delayed_action_count == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(pd_mode_local_active(PD_MODE_ARROW));
    CHECK(pd_mode_local_locked(PD_MODE_ARROW));
}

static void test_authored_layer_hold_releases_authored_pd_mode_child_cleanly(void) {
    keypos_t                             hold_pos     = test_keypos(1, 4);
    keypos_t                             child_pos    = test_keypos(1, 2);
    const key_runtime_integration_step_t hold_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_LAYER_HOLD_KEY, 1, 4),
        KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_TAP_HOLD_TERM + 1),
        KEY_RUNTIME_INTEGRATION_SCAN(),
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2),
    };
    const key_runtime_integration_step_t child_release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
    };
    const key_runtime_integration_step_t parent_release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_LAYER_HOLD_KEY, 1, 4),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(TEST_LAYER_HOLD_KEY).config != NULL);
    CHECK(key_behavior_lookup(VOLUME_MODE).config != NULL);

    key_runtime_integration_run(&fake_time, hold_steps, ARRAY_SIZE(hold_steps));
    CHECK(noah_runtime_debug_active_slot_count() == 2);
    CHECK(noah_runtime_debug_slot_owner_keycode(hold_pos) == TEST_LAYER_HOLD_KEY);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == VOLUME_MODE);
    CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == VOLUME_MODE);

    key_runtime_integration_run(&fake_time, child_release_steps, ARRAY_SIZE(child_release_steps));
    CHECK(noah_runtime_debug_active_slot_count() == 1);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == KC_NO);

    key_runtime_integration_run(&fake_time, parent_release_steps, ARRAY_SIZE(parent_release_steps));
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(hold_pos) == KC_NO);
}

static void test_authored_pd_mode_hold_releases_authored_pd_mode_child_cleanly(void) {
    keypos_t                             hold_pos     = test_keypos(1, 2);
    keypos_t                             child_pos    = test_keypos(2, 4);
    const key_runtime_integration_step_t hold_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2),
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_PRESS(PINCH_MODE, 2, 4),
    };
    const key_runtime_integration_step_t child_release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(PINCH_MODE, 2, 4),
    };
    const key_runtime_integration_step_t parent_release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
    };

    test_reset_state();
    test_configure_pinch_transparent_profile_path(child_pos);

    CHECK(key_behavior_lookup(VOLUME_MODE).config != NULL);
    CHECK(key_behavior_lookup(PINCH_MODE).config != NULL);

    key_runtime_integration_run(&fake_time, hold_steps, ARRAY_SIZE(hold_steps));
    CHECK(noah_runtime_debug_active_slot_count() == 2);
    CHECK(noah_runtime_debug_slot_owner_keycode(hold_pos) == VOLUME_MODE);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == PINCH_MODE);
    CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == PINCH_MODE);

    key_runtime_integration_run(&fake_time, child_release_steps, ARRAY_SIZE(child_release_steps));
    CHECK(noah_runtime_debug_active_slot_count() == 1);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == KC_NO);

    key_runtime_integration_run(&fake_time, parent_release_steps, ARRAY_SIZE(parent_release_steps));
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(hold_pos) == KC_NO);
}

static void test_authored_hold_action_activates_pd_mode_while_held(void) {
    keypos_t                             key_pos    = test_keypos(1, 3);
    const key_runtime_integration_step_t scenario[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_PD_HOLD_KEY, 1, 3), KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_TAP_HOLD_TERM + 1), KEY_RUNTIME_INTEGRATION_SCAN(), KEY_RUNTIME_INTEGRATION_RELEASE(TEST_PD_HOLD_KEY, 1, 3), KEY_RUNTIME_INTEGRATION_SCAN(),
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

    key_runtime_integration_run(&fake_time, &scenario[3], 2);
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    CHECK(reset_volume_count == 1);
    CHECK(current_cpi == default_dpi);
}

static void test_authored_double_tap_lock_locks_pd_mode(void) {
    const key_runtime_integration_step_t setup[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2),
        KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
        KEY_RUNTIME_INTEGRATION_ADVANCE(20),
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2),
    };
    const key_runtime_integration_step_t release[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2),
    };
    keypos_t key_pos = test_keypos(1, 2);

    test_reset_state();

    key_runtime_integration_run(&fake_time, setup, ARRAY_SIZE(setup));
    key_runtime_integration_run(&fake_time, release, ARRAY_SIZE(release));
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
        KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2), KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2), KEY_RUNTIME_INTEGRATION_ADVANCE(20), KEY_RUNTIME_INTEGRATION_PRESS(VOLUME_MODE, 1, 2), KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_TAP_HOLD_TERM + 1), KEY_RUNTIME_INTEGRATION_SCAN(), KEY_RUNTIME_INTEGRATION_RELEASE(VOLUME_MODE, 1, 2), KEY_RUNTIME_INTEGRATION_SCAN(),
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

    key_runtime_integration_run(&fake_time, &scenario[6], 2);
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    CHECK(current_cpi == default_dpi);
}

static void test_right_alt_single_tap_locks_arrow_mode_without_leaking_ralt_state(void) {
    keypos_t right_alt_pos = test_keypos(3, 5);

    test_reset_state();

    CHECK(key_behavior_lookup(KC_RIGHT_ALT).config != NULL);
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(fake_mods == 0);
    CHECK(fake_managed_mods == 0);
    CHECK(fake_physical_mods == 0);
    layer_state |= test_layer_mask(TEST_LAYER_POINTER);

    CHECK(!key_runtime_integration_process_record(KC_RIGHT_ALT, right_alt_pos, true));
    CHECK(noah_runtime_debug_slot_owner_keycode(right_alt_pos) == KC_RIGHT_ALT);
    CHECK(noah_runtime_debug_slot_tap_action(right_alt_pos) == ARROW_MODE_LOCK);
    CHECK(noah_runtime_debug_slot_held_action_keycode(right_alt_pos) == KC_NO);

    CHECK(!key_runtime_integration_process_record(KC_RIGHT_ALT, right_alt_pos, false));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
    CHECK(noah_runtime_debug_slot_owner_keycode(right_alt_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(right_alt_pos) == KC_NO);
    CHECK(fake_managed_mods == 0);
    CHECK(fake_physical_mods == 0);
    CHECK((fake_mods & MOD_BIT(KC_RIGHT_ALT)) == 0);
    CHECK((get_mods() & MOD_BIT(KC_RIGHT_ALT)) == 0);
    CHECK(auto_mouse_layer_off_count == 1);
    CHECK(auto_mouse_layer_off_active_slot_count == 0);
    CHECK(!auto_mouse_layer_off_pending_fallback);
    CHECK((auto_mouse_layer_off_real_mods & MOD_BIT(KC_RIGHT_ALT)) == 0);
    CHECK((auto_mouse_layer_off_managed_mods & MOD_BIT(KC_RIGHT_ALT)) == 0);
    CHECK((auto_mouse_layer_off_physical_mods & MOD_BIT(KC_RIGHT_ALT)) == 0);
    CHECK(layer_off_count == 1);
    CHECK(layer_state_set_count == 1);
    CHECK((layer_state & test_layer_mask(TEST_LAYER_POINTER)) == 0);
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));

    CHECK(key_runtime_integration_process_record(KC_C, test_keypos(0, 0), true));
    CHECK(key_runtime_integration_process_record(KC_C, test_keypos(0, 0), false));
}

static void test_right_alt_single_tap_uses_physical_trigger_half(void) {
    keypos_t left_pos = test_keypos(0, 0);

    test_reset_state();

    CHECK(!key_runtime_integration_process_record(KC_RIGHT_ALT, left_pos, true));
    CHECK(!key_runtime_integration_process_record(KC_RIGHT_ALT, left_pos, false));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);
    CHECK(pd_mode_display_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);
}

static void test_direct_pd_lock_press_uses_physical_trigger_half(void) {
    keypos_t left_pos = test_keypos(0, 1);

    test_reset_state();

    CHECK(!key_runtime_integration_process_record(ARROW_MODE_LOCK, left_pos, true));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);
    CHECK(pd_mode_display_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);
}

static void test_gui_double_tap_hold_with_right_alt_lock_child_keeps_runtime_quiescent(void) {
    keypos_t gui_pos       = test_keypos(0, 0);
    keypos_t right_alt_pos = test_keypos(3, 5);

    for (uint8_t index = 0; index < 2; index++) {
        bool release_gui_first = index != 0;

        test_reset_state();
        layer_state |= test_layer_mask(TEST_LAYER_POINTER);
        test_activate_gui_double_tap_alt_hold(gui_pos);

        CHECK(!key_runtime_integration_process_record(KC_RIGHT_ALT, right_alt_pos, true));
        CHECK(noah_runtime_debug_slot_owner_keycode(right_alt_pos) == KC_RIGHT_ALT);
        CHECK(noah_runtime_debug_slot_tap_action(right_alt_pos) == ARROW_MODE_LOCK);
        CHECK(noah_runtime_debug_slot_held_action_keycode(right_alt_pos) == KC_NO);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);

        if (release_gui_first) {
            CHECK(!key_runtime_integration_process_record(KC_LEFT_GUI, gui_pos, false));
            CHECK(noah_runtime_debug_slot_owner_keycode(gui_pos) == KC_NO);
            CHECK(noah_runtime_debug_slot_owner_keycode(right_alt_pos) == KC_RIGHT_ALT);
        }

        CHECK(!key_runtime_integration_process_record(KC_RIGHT_ALT, right_alt_pos, false));
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
        CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
        CHECK(noah_runtime_debug_slot_owner_keycode(right_alt_pos) == KC_NO);
        CHECK(noah_runtime_debug_slot_held_action_keycode(right_alt_pos) == KC_NO);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);

        if (!release_gui_first) {
            CHECK(!key_runtime_integration_process_record(KC_LEFT_GUI, gui_pos, false));
        }

        key_runtime_integration_scan();
        CHECK(noah_runtime_debug_active_slot_count() == 0);
        CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
        CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));
        CHECK((fake_mods & (MOD_BIT(KC_LEFT_GUI) | MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_RIGHT_ALT))) == 0);

        CHECK(key_runtime_integration_process_record(KC_C, test_keypos(0, 1), true));
        CHECK(key_runtime_integration_process_record(KC_C, test_keypos(0, 1), false));

        CHECK(pd_mode_toggle_lock_state(PD_MODE_ARROW));
        CHECK(pd_mode_local_active_snapshot() == 0);
        CHECK(pd_mode_local_locked_snapshot() == 0);
    }
}

static void test_dragscroll_child_overlap_stays_quiescent(uint16_t parent_keycode, keypos_t parent_pos, bool requires_parent_scan) {
    keypos_t child_pos = test_keypos(3, 5);

    for (uint8_t index = 0; index < 2; index++) {
        bool release_parent_first = index != 0;

        test_reset_state();
        test_set_keymap_key(TEST_LAYER_BASE, child_pos, KC_DOT);

        CHECK(!key_runtime_integration_process_record(parent_keycode, parent_pos, true));
        if (requires_parent_scan) {
            key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
            key_runtime_integration_scan();
        }

        CHECK(!key_runtime_integration_process_record(DRAGSCROLL, child_pos, true));
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
        CHECK(pd_mode_local_active(PD_MODE_DRAGSCROLL));
        CHECK(pd_mode_local_locked_snapshot() == 0);
        CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
        CHECK(reset_dragscroll_count == 0);

        key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
        key_runtime_integration_scan();
        CHECK(pd_mode_local_active(PD_MODE_DRAGSCROLL));
        CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);

        if (release_parent_first) {
            CHECK(!key_runtime_integration_process_record(parent_keycode, parent_pos, false));
            CHECK(noah_runtime_debug_slot_owner_keycode(parent_pos) == KC_NO);
            CHECK(pd_mode_local_active(PD_MODE_DRAGSCROLL));
        }

        CHECK(!key_runtime_integration_process_record(DRAGSCROLL, child_pos, false));
        CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);
        CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == KC_NO);

        if (!release_parent_first) {
            CHECK(!key_runtime_integration_process_record(parent_keycode, parent_pos, false));
        }

        key_runtime_integration_scan();
        CHECK(reset_dragscroll_count == 1);
        test_assert_pd_overlap_quiescent(parent_pos, child_pos);

        CHECK(key_runtime_integration_process_record(KC_C, test_keypos(0, 1), true));
        CHECK(key_runtime_integration_process_record(KC_C, test_keypos(0, 1), false));
        test_assert_pd_overlap_quiescent(parent_pos, child_pos);
    }
}

static void test_raw_lt_with_dragscroll_child_stays_quiescent(void) {
    test_dragscroll_child_overlap_stays_quiescent(LT(TEST_LAYER_NAV, KC_SLSH), test_keypos(1, 4), false);
}

static void test_authored_layer_hold_with_dragscroll_child_stays_quiescent(void) {
    test_dragscroll_child_overlap_stays_quiescent(TEST_LAYER_HOLD_KEY, test_keypos(1, 4), true);
}

static void test_pinch_single_tap_masks_mode_owned_gui_from_delayed_replay(void) {
    keypos_t                             key_pos       = test_keypos(2, 4);
    const key_runtime_integration_step_t press_steps[] = {
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
    keypos_t                             key_pos       = test_keypos(2, 4);
    const key_runtime_integration_step_t press_steps[] = {
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
                .type    = KEY_EVENT,
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
    keypos_t    pinch_key_pos = test_keypos(2, 4);
    keypos_t    plain_key_pos = test_keypos(0, 1);
    keyrecord_t plain_press   = test_record(plain_key_pos, true);

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
    keypos_t    pinch_key_pos = test_keypos(2, 4);
    keypos_t    gui_key_pos   = test_keypos(0, 0);
    keypos_t    plain_key_pos = test_keypos(0, 1);
    keyrecord_t gui_press     = test_record(gui_key_pos, true);
    keyrecord_t plain_press   = test_record(plain_key_pos, true);

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

static void test_gui_double_tap_hold_with_authored_pd_hold_keeps_processed_child_immediate(void) {
    keypos_t gui_pos   = test_keypos(0, 0);
    keypos_t hold_pos  = test_keypos(1, 2);
    keypos_t child_pos = test_keypos(1, 3);

    for (uint8_t index = 0; index < ARRAY_SIZE(test_release_orders); index++) {
        const test_release_order_case_t *release_order = &test_release_orders[index];

        test_reset_state();
        test_activate_gui_double_tap_alt_hold(gui_pos);

        CHECK(!key_runtime_integration_process_record(TEST_PD_HOLD_KEY, hold_pos, true));
        key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
        key_runtime_integration_scan();
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
        CHECK(pd_mode_local_active(PD_MODE_VOLUME));
        CHECK(noah_runtime_debug_slot_owner_keycode(hold_pos) == TEST_PD_HOLD_KEY);
        CHECK(noah_runtime_debug_slot_held_action_keycode(hold_pos) == VOLUME_MODE);

        CHECK(!key_runtime_integration_process_record(TEST_HANDLED_TAP_KEY, child_pos, true));

        for (uint8_t release_index = 0; release_index < ARRAY_SIZE(release_order->order); release_index++) {
            switch (release_order->order[release_index]) {
                case TEST_RELEASE_TARGET_CHILD: {
                    uint16_t previous_tap_count     = tap_code16_count;
                    uint16_t previous_delayed_count = delayed_action_count;

                    CHECK(!key_runtime_integration_process_record(TEST_HANDLED_TAP_KEY, child_pos, false));
                    CHECK(tap_code16_count == (uint16_t)(previous_tap_count + 1));
                    CHECK(last_tap_code16 == KC_J);
                    CHECK(delayed_action_count == previous_delayed_count);
                    break;
                }
                case TEST_RELEASE_TARGET_PARENT:
                    CHECK(!key_runtime_integration_process_record(TEST_PD_HOLD_KEY, hold_pos, false));
                    break;
                case TEST_RELEASE_TARGET_GUI:
                    CHECK(!key_runtime_integration_process_record(KC_LEFT_GUI, gui_pos, false));
                    break;
            }
        }

        key_runtime_integration_scan();
        test_assert_gui_pd_overlap_quiescent(gui_pos, hold_pos);

        CHECK(!key_runtime_integration_process_record(TEST_HANDLED_TAP_KEY, child_pos, true));
        CHECK(!key_runtime_integration_process_record(TEST_HANDLED_TAP_KEY, child_pos, false));
        CHECK(last_tap_code16 == KC_J);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
        test_assert_gui_pd_overlap_quiescent(gui_pos, hold_pos);
    }
}

int main(void) {
    test_authored_single_press_preserves_default_pd_mode_hold();
    test_authored_single_press_pd_mode_hold_dispatches_plain_taps_immediately();
    test_raw_lt_hold_dispatches_authored_tap_key_immediately();
    test_raw_lt_hold_dispatches_authored_plain_tap_immediately();
    test_authored_layer_hold_dispatches_authored_tap_key_immediately();
    test_authored_layer_hold_dispatches_authored_plain_tap_immediately();
    test_interrupted_locked_pd_mode_press_still_toggles_lock_on_release();
    test_authored_pd_mode_hold_dispatches_authored_tap_key_immediately();
    test_authored_layer_hold_releases_authored_pd_mode_child_cleanly();
    test_authored_pd_mode_hold_releases_authored_pd_mode_child_cleanly();
    test_authored_hold_action_activates_pd_mode_while_held();
    test_authored_double_tap_lock_locks_pd_mode();
    test_authored_second_press_hold_branches_into_other_pd_mode();
    test_right_alt_single_tap_locks_arrow_mode_without_leaking_ralt_state();
    test_right_alt_single_tap_uses_physical_trigger_half();
    test_direct_pd_lock_press_uses_physical_trigger_half();
    test_gui_double_tap_hold_with_right_alt_lock_child_keeps_runtime_quiescent();
    test_raw_lt_with_dragscroll_child_stays_quiescent();
    test_authored_layer_hold_with_dragscroll_child_stays_quiescent();
    test_pinch_single_tap_masks_mode_owned_gui_from_delayed_replay();
    test_pinch_single_tap_preserves_physically_held_gui_on_delayed_replay();
    test_pinch_masks_mode_owned_gui_during_concurrent_plain_key_processing();
    test_pinch_keeps_physically_held_gui_visible_during_concurrent_plain_key_processing();
    test_gui_double_tap_hold_with_authored_pd_hold_keeps_processed_child_immediate();

    puts("pd_mode_key_runtime_integration host tests passed");
    return 0;
}
