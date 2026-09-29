#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NOAH_PD_PROFILE_ENABLE
#include "users/noah/lib/profile/runtime/effective_pd_runtime.h"
#include "users/noah/lib/pointing/modes/pd_mode_configured.h"
#endif

#include "key_runtime_integration_harness.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/lib/key/behavior/key_behavior.h"
#include "users/noah/lib/key/behavior/key_behavior_lookup.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/key/runtime/projection/projection.h"
#include "users/noah/lib/key/runtime/reducer/ownership_state.h"
#include "users/noah/lib/key/runtime/reducer/runtime.h"
#include "users/noah/lib/key/runtime/reducer/state_query.h"
#include "users/noah/lib/state/diagnostics/runtime_debug.h"
#include "users/noah/lib/state/shared/runtime_reset.h"
#include "users/noah/noah_runtime.h"
#include "users/noah/lib/state/ownership/layer_ownership.h"
#ifdef NOAH_TEST_QMK_GESTURES
#    include "users/noah/lib/compat/qmk_combo_origin.h"
#endif

#ifndef KC_J
#    define KC_J 0x000Du
#endif
#ifndef KC_SLSH
#    define KC_SLSH 0x0038u
#endif
#ifndef KC_DOT
#    define KC_DOT 0x0037u
#endif
#define TEST_PINCH_ZOOM_TAP VIA_MACRO_6
#if defined(PD_MODE_KEY_RUNTIME_TEST_LEGACY_PINCH_IMPLICIT)
#    define TEST_PINCH_SINGLE_STEP {.tap = TAP_SENDS(KC_TRNS)}
#else
#    define TEST_PINCH_SINGLE_STEP {.tap = TAP_SENDS(KC_TRNS), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(PD_SLOT_5)}
#endif

enum {
    TEST_PD_HOLD_KEY       = CUSTOM_KEY_0 + 0x10,
    TEST_HANDLED_TAP_KEY   = CUSTOM_KEY_0 + 0x11,
    TEST_LAYER_HOLD_KEY    = CUSTOM_KEY_0 + 0x12,
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
#ifdef NOAH_TEST_QMK_GESTURES
    {.keycode = MT(MOD_LCTL, KC_A), .tap_hold_term = 100, .tap_counts[0] = {.tap = TAP_SENDS(KC_B), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MS_BTN7)}},
    {.keycode = TT(2), .tap_hold_term = 100, .tap_counts[0] = {.tap = TAP_SENDS(KC_B), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MS_BTN7)}},
    {.keycode = OSL(2), .tap_hold_term = 100, .tap_counts[0] = {.tap = TAP_SENDS(KC_B), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MS_BTN7)}},
    {.keycode = OSM(MOD_LSFT), .tap_hold_term = 100, .tap_counts[0] = {.tap = TAP_SENDS(KC_B), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MS_BTN7)}},
    // Sparse rows: only a repeated tap is authored.
    {.keycode = MT(MOD_LSFT | MOD_LGUI, KC_S), .tap_hold_term = 100, .tap_counts[1] = {.tap = TAP_SENDS(KC_X)}},
    {.keycode = OSM(MOD_LALT), .tap_hold_term = 100, .tap_counts[1] = {.tap = TAP_SENDS(KC_Y)}},
#endif
    {
        .keycode        = KC_RIGHT_ALT,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts[0] =
            {
                .tap = TAP_SENDS(PD_SLOT_4_LOCK),
            },
    },
    {
        .keycode        = KC_LEFT_GUI,
#ifdef NOAH_TEST_QMK_GESTURES
        .tap_hold_term  = 150, // Screenshot: inherited default.
        .tap_counts[2] = {.tap = TAP_SENDS(OSM(MOD_LSFT))},
#else
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
#endif
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
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(PD_SLOT_1),
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
        .keycode        = PD_SLOT_1,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts[1] =
            {
                .tap  = TAP_SENDS(PD_SLOT_1_LOCK),
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(PD_SLOT_2),
            },
    },
    {
        .keycode        = PD_SLOT_5,
        .tap_hold_term  = TEST_PD_TAP_HOLD_TERM,
        .multi_tap_term = TEST_PD_MULTI_TAP_TERM,
        .tap_counts =
            {
                [0] = TEST_PINCH_SINGLE_STEP,
                [1] = {.tap = TAP_SENDS(TEST_PINCH_ZOOM_TAP), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(PD_SLOT_3)},
            },
    },
    // The factory row: a single press falls back to holding the button itself.
    {.keycode = MS_BTN3, .multi_tap_term = 100, .tap_hold_term = 100, .tap_counts = {[1] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(MS_BTN7)}}},
    {.keycode = MS_BTN1, .tap_hold_term = TEST_PD_TAP_HOLD_TERM, .multi_tap_term = TEST_PD_MULTI_TAP_TERM, .tap_counts[0] = {.tap = TAP_SENDS(KC_A), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LEFT_ALT)}},
    {.keycode = MS_BTN2, .tap_hold_term = TEST_PD_TAP_HOLD_TERM, .multi_tap_term = TEST_PD_MULTI_TAP_TERM, .tap_counts[0] = {.hold = REPEAT_WHILE_HELD(KC_B, 20)}},
};
const uint8_t key_behavior_count = ARRAY_SIZE(key_behaviors);

static uint16_t              fake_time;
static uint16_t              current_cpi;
static uint16_t              default_dpi;
static uint16_t              sniping_dpi;
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

// Run the multi-tap window out so the pending series flushes and its branch is
// entered. There is no separate confirm window after it any more.
static void test_flush_pending_tap_branch(void) {
    key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM + 1);
    key_runtime_integration_scan();
}

#if defined(PD_MODE_KEY_RUNTIME_TEST_LEGACY_PINCH_IMPLICIT)
static bool test_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}
#endif

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
    test_set_keymap_key(TEST_LAYER_POINTER, key_pos, PD_SLOT_5);
    test_set_keymap_key(TEST_LAYER_BASE, key_pos, LT(TEST_LAYER_SYM, KC_J));
    layer_state = test_layer_mask(TEST_LAYER_BASE) | test_layer_mask(TEST_LAYER_POINTER);
}

#ifdef NOAH_PD_PROFILE_ENABLE
static uint8_t configured_pd_bytes[NOAH_PROFILE_PD_V1_SIZE];
static bool configured_pd_read(void *context, size_t offset, uint8_t *target, size_t length) {
    (void)context;
    if (offset > sizeof(configured_pd_bytes) || length > sizeof(configured_pd_bytes) - offset) return false;
    memcpy(target, configured_pd_bytes + offset, length);
    return true;
}
static void publish_configured_pd(void) {
    noah_effective_profile_snapshot_t view = {0};
    view.reader = (noah_profile_reader_t){.read = configured_pd_read, .length = sizeof(configured_pd_bytes)};
    view.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD;
    view.profile.pd.length = sizeof(configured_pd_bytes);
    noah_effective_pd_invalidate(NULL, 0, view.identity, view.identity, &view);
    CHECK(noah_effective_pd_ready());
}
static void load_configured_pd(void) {
    noah_pd_engine_exit();
    memset(configured_pd_bytes, 0, sizeof(configured_pd_bytes));
    configured_pd_bytes[0] = 1; configured_pd_bytes[1] = 8; configured_pd_bytes[2] = 96;
    for (uint8_t i = 0; i < 8; i++) noah_profile_pd_v1_encode_record(&noah_pd_defaults[i], configured_pd_bytes + 8 + i * 96);
    publish_configured_pd();
}
report_mouse_t noah_pd_configured_scroll(const uint8_t *record, report_mouse_t report) {
    (void)record;
    return report; // Motion parity is covered by pd_mode_handlers_test.
}
// Configured directional modes reset their shared engine, never legacy state.
#define EXPECT_VOLUME_RESETS(count) CHECK(reset_volume_count == 0)
#else
#define EXPECT_VOLUME_RESETS(count) CHECK(reset_volume_count == (count))
#endif

static void test_reset_state(void) {
#ifdef NOAH_PD_PROFILE_ENABLE
    load_configured_pd();
#endif
    noah_runtime_reset_for_test();
#ifdef NOAH_TEST_QMK_GESTURES
    noah_qmk_combo_origin_reset();
#endif
    test_reset_keymap();

    fake_time                              = 1000;
    current_cpi                            = 0;
    default_dpi                            = 900;
    sniping_dpi                            = 350;
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

static __attribute__((unused)) keyrecord_t test_record(keypos_t key_pos, bool pressed) {
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

#ifdef NOAH_TEST_QMK_GESTURES
uint16_t gesture_keycode(uint8_t row, uint8_t col);
#endif
uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
#ifdef NOAH_TEST_QMK_GESTURES
    // Userspace combo origin must see the same members QMK's combo engine does.
    if (row == 3 || (row >= 4 && row < 6)) return gesture_keycode(row, column);
#endif
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

#ifdef NOAH_TEST_QMK_GESTURES
static uint16_t gesture_registered;
static uint16_t gesture_unregistered;
#endif
void register_code16(uint16_t keycode) {
#ifdef NOAH_TEST_QMK_GESTURES
    gesture_registered = keycode;
#endif
    (void)keycode;
}

void unregister_code16(uint16_t keycode) {
#ifdef NOAH_TEST_QMK_GESTURES
    gesture_unregistered = keycode;
#endif
    (void)keycode;
}

void wait_ms(uint16_t ms) {
    (void)ms;
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

// The ledger is faked here, so a literal tap is recorded as QMK's tap.
void owned_keycode_tap_literal(uint16_t keycode) {
    tap_code16(keycode);
}

bool owned_keycode_is_supported(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease) {
    (void)keycode;
    (void)lease;
    return false;
}

bool owned_keycode_release(owned_keycode_lease_t *lease) {
    (void)lease;
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

#ifdef NOAH_TEST_QMK_GESTURES
static uint16_t gesture_qmk_tap;
static unsigned gesture_qmk_tap_count;
#endif
void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
#ifdef NOAH_TEST_QMK_GESTURES
    gesture_qmk_tap = keycode;
    gesture_qmk_tap_count++;
#endif
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

void keyboard_mod_ownership_track_report_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

void keyboard_mod_ownership_track_mod_tap_hold_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
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

bool keyboard_mod_ownership_register_mods(uint8_t mods) {
    uint8_t added = (uint8_t)(mods & ~fake_mods);

    fake_managed_mods |= mods;
    fake_mods |= mods;
    return added != 0u;
}

void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    fake_managed_mods &= (uint8_t)~mods;
    fake_mods = (uint8_t)((fake_mods & (uint8_t)~mods) | fake_physical_mods | fake_managed_mods);
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    if (IS_MODIFIER_KEYCODE(keycode)) {
        keyboard_mod_ownership_register_mods(MOD_BIT(keycode));
    }
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    if (IS_MODIFIER_KEYCODE(keycode)) {
        keyboard_mod_ownership_unregister_mods(MOD_BIT(keycode));
    }
}

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    return (uint8_t)(mods & fake_managed_mods & (uint8_t)~fake_physical_mods);
}

#ifdef NOAH_TEST_QMK_GESTURES
static uint8_t gesture_momentary_layer;
#endif
void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
#ifdef NOAH_TEST_QMK_GESTURES
    gesture_momentary_layer = layer;
#endif
    (void)key_pos;
    (void)layer;
}

bool layer_ownership_is_locked(uint8_t layer) {
    (void)layer;
    return false;
}

#ifdef NOAH_TEST_QMK_GESTURES
static layer_state_t gesture_locks;
#endif
bool layer_ownership_toggle_lock_state(uint8_t layer) {
#ifdef NOAH_TEST_QMK_GESTURES
    gesture_locks ^= (layer_state_t)1u << layer;
#endif
    (void)layer;
    return true;
}

bool layer_ownership_goto(uint8_t layer) {
    (void)layer;
    return true;
}

bool is_oneshot_enabled(void) {
    return true;
}

bool layer_ownership_oneshot_tap(uint8_t layer, uint16_t now, uint16_t double_tap_ms) {
    (void)now;
    (void)double_tap_ms;
    (void)layer;
    return false;
}

bool layer_ownership_oneshot_consume(void) {
    return false;
}

uint8_t layer_ownership_oneshot_layer(void) {
    return UINT8_MAX;
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

bool get_auto_mouse_enable(void) {
    return auto_mouse_enabled;
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

void auto_mouse_reset_trigger(bool pressed) {
    (void)pressed;
}

uint8_t read_source_layers_cache(keypos_t key) {
    (void)key;
    return 0;
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

uint16_t charybdis_get_pointer_sniping_dpi(void) {
    return sniping_dpi;
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

// Historical key-event fake for the action lifecycle scenarios below. The
// configured firmware routes button overrides through the shared engine.
bool handle_arrow_mode_key(uint16_t keycode, keyrecord_t *record) {
    if (keycode < QK_MOUSE_BUTTON_1 || keycode > QK_MOUSE_BUTTON_1 + 2u) {
        return false;
    }
    if (record->event.pressed && keycode == MS_BTN2) {
        noah_emit_literal_tap(G(KC_C), NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS);
    }
    if (record->event.pressed && keycode == MS_BTN3) {
        noah_emit_literal_tap(G(KC_V), NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS);
    }
    return true;
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
    test_flush_pending_tap_branch();
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

#if defined(PD_MODE_KEY_RUNTIME_TEST_LEGACY_PINCH_IMPLICIT)
static void test_assert_active_pd_owner_invariant(keypos_t owner_pos, uint16_t owner_keycode, uint16_t held_action, pd_mode_mask_t mode, bool expect_pinch_gui) {
    projection_snapshot_t snapshot = key_runtime_core_projection_snapshot_capture();
    keypos_t              pd_owner = {0};
    uint8_t               gui_mask = MOD_BIT(KC_LEFT_GUI);

    CHECK(mode != 0);
    CHECK(pd_mode_local_active_snapshot() == mode);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(pd_mode_local_owner_key_pos_snapshot(&pd_owner));
    CHECK(test_keypos_equal(pd_owner, owner_pos));
    CHECK(noah_runtime_debug_slot_owner_keycode(owner_pos) == owner_keycode);
    CHECK(noah_runtime_debug_slot_held_action_keycode(owner_pos) == held_action);
    CHECK(snapshot.core_shadow_pd_mode_local_active == mode);
    CHECK(snapshot.core_shadow_pointer_pd_mode_anchor_active == pd_mode_has_trait(mode, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED));
    CHECK((fake_managed_mods & gui_mask) == (expect_pinch_gui ? gui_mask : 0u));
}

static void test_assert_no_pd_owner_invariant(void) {
    projection_snapshot_t snapshot = key_runtime_core_projection_snapshot_capture();
    keypos_t              pd_owner = {0};

    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(!pd_mode_local_owner_key_pos_snapshot(&pd_owner));
    CHECK(snapshot.core_shadow_pd_mode_local_active == 0);
    CHECK(!snapshot.core_shadow_pointer_pd_mode_anchor_active);
    CHECK((fake_managed_mods & MOD_BIT(KC_LEFT_GUI)) == 0);
}
#endif

static void test_authored_single_press_preserves_default_pd_mode_hold(void) {
    keypos_t                             key_pos       = test_keypos(1, 2);
    const key_runtime_integration_step_t press_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_1, 1, 2),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_1, 1, 2),
        KEY_RUNTIME_INTEGRATION_SCAN(),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(PD_SLOT_1).config != NULL);
    CHECK(pd_mode_for_keycode(PD_SLOT_1) == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active_snapshot() == 0);

    key_runtime_integration_run(&fake_time, press_steps, ARRAY_SIZE(press_steps));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == PD_SLOT_1);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == PD_SLOT_1);
    CHECK(split_sync_count == 0);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    const press_token_t *release_token = key_runtime_core_press_token_at(key_pos);

    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(!pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    EXPECT_VOLUME_RESETS(1);
    CHECK(current_cpi == default_dpi);
}

static void test_authored_single_press_pd_mode_hold_dispatches_plain_taps_immediately(void) {
    keypos_t                             key_pos              = test_keypos(1, 2);
    const key_runtime_integration_step_t hold_and_tap_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_1, 1, 2),
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_PRESS(TEST_HANDLED_TAP_KEY, 1, 3),
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_HANDLED_TAP_KEY, 1, 3),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_1, 1, 2),
    };

    test_reset_state();

    key_runtime_integration_run(&fake_time, hold_and_tap_steps, ARRAY_SIZE(hold_and_tap_steps));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == PD_SLOT_1);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == PD_SLOT_1);
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

static void test_interrupted_locked_pd_mode_press_unlocks_on_press_and_releases_momentary_hold(void) {
    keypos_t                             key_pos               = test_keypos(1, 2);
    const key_runtime_integration_step_t press_and_interrupt[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_1, 1, 2),
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_PRESS(KC_J, 1, 3),
        KEY_RUNTIME_INTEGRATION_RELEASE(KC_J, 1, 3),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_1, 1, 2),
    };

    test_reset_state();

    CHECK(pd_mode_set_lock_state(PD_MODE_VOLUME, true));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    EXPECT_VOLUME_RESETS(0);

    key_runtime_integration_run(&fake_time, press_and_interrupt, ARRAY_SIZE(press_and_interrupt));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(!pd_mode_local_locked(PD_MODE_VOLUME));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == PD_SLOT_1);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == PD_SLOT_1);
    EXPECT_VOLUME_RESETS(1);

    key_runtime_integration_run(&fake_time, release_steps, ARRAY_SIZE(release_steps));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(!pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(!pd_mode_local_locked(PD_MODE_VOLUME));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    EXPECT_VOLUME_RESETS(2);
}

static void test_authored_pd_mode_hold_dispatches_authored_tap_key_immediately(void) {
    keypos_t                             hold_pos     = test_keypos(1, 2);
    keypos_t                             child_pos    = test_keypos(3, 5);
    const key_runtime_integration_step_t hold_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_1, 1, 2),
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_PRESS(KC_RIGHT_ALT, 3, 5),
        KEY_RUNTIME_INTEGRATION_RELEASE(KC_RIGHT_ALT, 3, 5),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_1, 1, 2),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(PD_SLOT_1).config != NULL);
    CHECK(key_behavior_lookup(KC_RIGHT_ALT).config != NULL);

    key_runtime_integration_run(&fake_time, hold_steps, ARRAY_SIZE(hold_steps));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
    CHECK(delayed_action_count == 0);
    CHECK(noah_runtime_debug_slot_held_action_keycode(hold_pos) == KC_NO);
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
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_1, 1, 2),
    };
    const key_runtime_integration_step_t child_release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_1, 1, 2),
    };
    const key_runtime_integration_step_t parent_release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(TEST_LAYER_HOLD_KEY, 1, 4),
    };

    test_reset_state();

    CHECK(key_behavior_lookup(TEST_LAYER_HOLD_KEY).config != NULL);
    CHECK(key_behavior_lookup(PD_SLOT_1).config != NULL);

    key_runtime_integration_run(&fake_time, hold_steps, ARRAY_SIZE(hold_steps));
    CHECK(noah_runtime_debug_active_slot_count() == 2);
    CHECK(noah_runtime_debug_slot_owner_keycode(hold_pos) == TEST_LAYER_HOLD_KEY);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == PD_SLOT_1);
    CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == PD_SLOT_1);

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
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_1, 1, 2), KEY_RUNTIME_INTEGRATION_ADVANCE(10), KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_5, 2, 4), KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_TAP_HOLD_TERM + 1), KEY_RUNTIME_INTEGRATION_SCAN(),
    };
    const key_runtime_integration_step_t child_release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_5, 2, 4),
    };
    const key_runtime_integration_step_t parent_release_steps[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_1, 1, 2),
    };

    test_reset_state();
    test_configure_pinch_transparent_profile_path(child_pos);

    CHECK(key_behavior_lookup(PD_SLOT_1).config != NULL);
    CHECK(key_behavior_lookup(PD_SLOT_5).config != NULL);

    key_runtime_integration_run(&fake_time, hold_steps, ARRAY_SIZE(hold_steps));
    CHECK(noah_runtime_debug_active_slot_count() == 2);
    CHECK(noah_runtime_debug_slot_owner_keycode(hold_pos) == PD_SLOT_1);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == PD_SLOT_5);
    CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == PD_SLOT_5);

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
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == PD_SLOT_1);

    key_runtime_integration_run(&fake_time, &scenario[3], 2);
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    EXPECT_VOLUME_RESETS(1);
    CHECK(current_cpi == default_dpi);
}

static void test_authored_double_tap_lock_locks_pd_mode(void) {
    const key_runtime_integration_step_t setup[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_1, 1, 2),
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_1, 1, 2),
        KEY_RUNTIME_INTEGRATION_ADVANCE(20),
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_1, 1, 2),
    };
    const key_runtime_integration_step_t release[] = {
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_1, 1, 2),
    };
    keypos_t key_pos = test_keypos(1, 2);

    test_reset_state();

    key_runtime_integration_run(&fake_time, setup, ARRAY_SIZE(setup));
    key_runtime_integration_run(&fake_time, release, ARRAY_SIZE(release));
    test_flush_pending_tap_branch();
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked(PD_MODE_VOLUME));
    CHECK(noah_runtime_debug_slot_owner_keycode(test_keypos(1, 2)) == KC_NO);
    EXPECT_VOLUME_RESETS(1);
}

static void test_authored_second_press_hold_branches_into_other_pd_mode(void) {
    keypos_t                             key_pos    = test_keypos(1, 2);
    const key_runtime_integration_step_t scenario[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_1, 1, 2), KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_1, 1, 2), KEY_RUNTIME_INTEGRATION_ADVANCE(20), KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_1, 1, 2), KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_TAP_HOLD_TERM + 1), KEY_RUNTIME_INTEGRATION_SCAN(), KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_1, 1, 2), KEY_RUNTIME_INTEGRATION_SCAN(),
    };

    test_reset_state();

    key_runtime_integration_run(&fake_time, scenario, 6);
    test_flush_pending_tap_branch();
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_BRIGHTNESS);
    CHECK(pd_mode_local_active(PD_MODE_BRIGHTNESS));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(!pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == PD_SLOT_1);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == PD_SLOT_2);
    EXPECT_VOLUME_RESETS(1);

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
    CHECK(noah_runtime_debug_slot_tap_action(right_alt_pos) == PD_SLOT_4_LOCK);
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

    CHECK(!key_runtime_integration_process_record(PD_SLOT_4_LOCK, left_pos, true));
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
        CHECK(noah_runtime_debug_slot_tap_action(right_alt_pos) == PD_SLOT_4_LOCK);
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

        CHECK(!key_runtime_integration_process_record(PD_SLOT_0, child_pos, true));
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
        CHECK(pd_mode_local_active(PD_MODE_DRAGSCROLL));
        CHECK(pd_mode_local_locked_snapshot() == 0);
        CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == PD_SLOT_0);
        CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == PD_SLOT_0);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
#ifdef NOAH_PD_PROFILE_ENABLE
        CHECK(reset_dragscroll_count == 1); // Entry clears the shared scroll accumulator.
#else
        CHECK(reset_dragscroll_count == 0);
#endif

        key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
        key_runtime_integration_scan();
        CHECK(pd_mode_local_active(PD_MODE_DRAGSCROLL));
        CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == PD_SLOT_0);
        CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == PD_SLOT_0);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);

        if (release_parent_first) {
            CHECK(!key_runtime_integration_process_record(parent_keycode, parent_pos, false));
            CHECK(noah_runtime_debug_slot_owner_keycode(parent_pos) == KC_NO);
            CHECK(pd_mode_local_active(PD_MODE_DRAGSCROLL));
        }

        CHECK(!key_runtime_integration_process_record(PD_SLOT_0, child_pos, false));
        CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);
        CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == KC_NO);

        if (!release_parent_first) {
            CHECK(!key_runtime_integration_process_record(parent_keycode, parent_pos, false));
        }

        key_runtime_integration_scan();
#ifdef NOAH_PD_PROFILE_ENABLE
        CHECK(reset_dragscroll_count == 2); // Entry and exit each reset the engine.
#else
        CHECK(reset_dragscroll_count == 1);
#endif
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

#if !defined(PD_MODE_KEY_RUNTIME_TEST_LEGACY_PINCH_IMPLICIT)
static void test_pinch_single_tap_defers_mode_owned_gui_from_delayed_replay(void) {
    keypos_t                             key_pos       = test_keypos(2, 4);
    const key_runtime_integration_step_t press_steps[] = {
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_5, 2, 4),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_5, 2, 4),
    };
    const key_runtime_integration_step_t flush_steps[] = {
        KEY_RUNTIME_INTEGRATION_ADVANCE(TEST_PD_MULTI_TAP_TERM + 1),
        KEY_RUNTIME_INTEGRATION_SCAN(),
    };

    test_reset_state();
    test_configure_pinch_transparent_profile_path(key_pos);
    fake_mods = MOD_BIT(KC_LEFT_SHIFT);

    CHECK(key_behavior_lookup(PD_SLOT_5).config != NULL);
    CHECK(pd_mode_for_keycode(PD_SLOT_5) == PD_MODE_PINCH);

    key_runtime_integration_run(&fake_time, press_steps, ARRAY_SIZE(press_steps));
    CHECK(!pd_mode_local_active(PD_MODE_PINCH));
    CHECK(fake_mods == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == PD_SLOT_5);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);

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
        KEY_RUNTIME_INTEGRATION_PRESS(PD_SLOT_5, 2, 4),
    };
    const key_runtime_integration_step_t release_steps[] = {
        KEY_RUNTIME_INTEGRATION_ADVANCE(10),
        KEY_RUNTIME_INTEGRATION_RELEASE(PD_SLOT_5, 2, 4),
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
    CHECK(!pd_mode_local_active(PD_MODE_PINCH));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == PD_SLOT_5);

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

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_key_pos, true));
    key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();
    test_flush_pending_tap_branch();
    CHECK(pd_mode_local_active(PD_MODE_PINCH));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    CHECK(noah_pre_process_record_user(KC_C, &plain_press));
    CHECK(noah_process_record_user(KC_C, &plain_press));
    CHECK(fake_mods == MOD_BIT(KC_LEFT_SHIFT));

    noah_post_process_record_user(KC_C, &plain_press);
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_key_pos, false));
    CHECK(!pd_mode_local_active(PD_MODE_PINCH));
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

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_key_pos, true));
    key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();
    CHECK(pd_mode_local_active(PD_MODE_PINCH));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    CHECK(noah_pre_process_record_user(KC_LEFT_GUI, &gui_press));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    CHECK(noah_pre_process_record_user(KC_C, &plain_press));
    CHECK(noah_process_record_user(KC_C, &plain_press));
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    noah_post_process_record_user(KC_C, &plain_press);
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_GUI)));

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_key_pos, false));
    CHECK(!pd_mode_local_active(PD_MODE_PINCH));
}

static void test_pinch_double_tap_hold_zoom_branch_keeps_projection_coherent(void) {
    keypos_t              key_pos = test_keypos(2, 4);
    projection_snapshot_t snapshot;

    test_reset_state();
    test_configure_pinch_transparent_profile_path(key_pos);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, key_pos, true));
    key_runtime_integration_advance(&fake_time, 10);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, key_pos, false));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 1);

    key_runtime_integration_advance(&fake_time, 20);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, key_pos, true));
    key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();
    test_flush_pending_tap_branch();

    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ZOOM);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == PD_SLOT_5);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == PD_SLOT_3);
    CHECK(fake_managed_mods == 0);
    CHECK((fake_mods & MOD_BIT(KC_LEFT_GUI)) == 0);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == PD_MODE_ZOOM);
    CHECK(snapshot.core_shadow_pointer_pd_mode_anchor_active);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, key_pos, false));
    key_runtime_integration_scan();

    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    CHECK(fake_managed_mods == 0);
    CHECK(fake_mods == 0);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == 0);
    CHECK(!snapshot.core_shadow_pointer_pd_mode_anchor_active);
}

static void test_volume_pinch_zoom_volume_alternation_keeps_projection_coherent(void) {
    keypos_t              volume_pos = test_keypos(1, 2);
    keypos_t              pinch_pos  = test_keypos(2, 4);
    projection_snapshot_t snapshot;

    test_reset_state();
    test_configure_pinch_transparent_profile_path(pinch_pos);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_1, volume_pos, true));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == PD_MODE_VOLUME);

    key_runtime_integration_advance(&fake_time, 10);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_1, volume_pos, false));
    key_runtime_integration_scan();
    CHECK(pd_mode_local_active_snapshot() == 0);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == 0);

    key_runtime_integration_advance(&fake_time, 20);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
    key_runtime_integration_advance(&fake_time, 10);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
    CHECK(pd_mode_local_active_snapshot() == 0);

    key_runtime_integration_advance(&fake_time, 20);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
    key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();
    test_flush_pending_tap_branch();
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ZOOM);
    CHECK(noah_runtime_debug_slot_owner_keycode(pinch_pos) == PD_SLOT_5);
    CHECK(noah_runtime_debug_slot_held_action_keycode(pinch_pos) == PD_SLOT_3);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == PD_MODE_ZOOM);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
    key_runtime_integration_scan();
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(pinch_pos) == KC_NO);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == 0);

    key_runtime_integration_advance(&fake_time, 20);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_1, volume_pos, true));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == PD_MODE_VOLUME);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_1, volume_pos, false));
    key_runtime_integration_scan();
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(fake_managed_mods == 0);
    CHECK(fake_mods == 0);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == 0);
    CHECK(!snapshot.core_shadow_pointer_pd_mode_anchor_active);
}

static void test_pinch_double_tap_salvos_stay_quiescent(void) {
    keypos_t              pinch_pos = test_keypos(2, 4);
    projection_snapshot_t snapshot;

    test_reset_state();
    test_configure_pinch_transparent_profile_path(pinch_pos);

    for (uint8_t salvo = 0; salvo < 4; salvo++) {
        CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
        key_runtime_integration_advance(&fake_time, 10);
        CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
        key_runtime_integration_scan();

        CHECK(pd_mode_local_active_snapshot() == 0);
        CHECK(noah_runtime_debug_active_slot_count() == 0);
        CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 1);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
        snapshot = key_runtime_core_projection_snapshot_capture();
        CHECK(snapshot.core_shadow_pd_mode_local_active == 0);

        key_runtime_integration_advance(&fake_time, 20);
        CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
        key_runtime_integration_advance(&fake_time, 10);
        CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
        key_runtime_integration_scan();

        CHECK(pd_mode_local_active_snapshot() == 0);
        CHECK(noah_runtime_debug_active_slot_count() == 0);
        CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 1);
        CHECK(delayed_action_count == salvo);

        test_flush_pending_tap_branch();

        CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
        CHECK(noah_runtime_debug_slot_owner_keycode(pinch_pos) == KC_NO);
        CHECK(noah_runtime_debug_slot_held_action_keycode(pinch_pos) == KC_NO);
        CHECK(delayed_action_count == (uint8_t)(salvo + 1u));
        CHECK(last_delayed_action == TEST_PINCH_ZOOM_TAP);
        CHECK(fake_managed_mods == 0);
        CHECK(fake_mods == 0);
        snapshot = key_runtime_core_projection_snapshot_capture();
        CHECK(snapshot.core_shadow_pd_mode_local_active == 0);
        CHECK(!snapshot.core_shadow_pointer_pd_mode_anchor_active);

        key_runtime_integration_advance(&fake_time, 40);
    }
}
#endif

#if defined(PD_MODE_KEY_RUNTIME_TEST_LEGACY_PINCH_IMPLICIT)
static void test_legacy_pinch_preempted_by_volume_clears_stale_pinch_owner(void) {
    keypos_t pinch_pos  = test_keypos(2, 4);
    keypos_t volume_pos = test_keypos(1, 2);

    test_reset_state();
    test_configure_pinch_transparent_profile_path(pinch_pos);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
    test_assert_no_pd_owner_invariant();
    key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();
    test_assert_active_pd_owner_invariant(pinch_pos, PD_SLOT_5, PD_SLOT_5, PD_MODE_PINCH, true);

    key_runtime_integration_advance(&fake_time, 10);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_1, volume_pos, true));

    test_assert_active_pd_owner_invariant(volume_pos, PD_SLOT_1, PD_SLOT_1, PD_MODE_VOLUME, false);
    CHECK(noah_runtime_debug_slot_owner_keycode(pinch_pos) == PD_SLOT_5);
    CHECK(noah_runtime_debug_slot_held_action_keycode(pinch_pos) == KC_NO);
    CHECK((fake_mods & MOD_BIT(KC_LEFT_GUI)) == 0);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
    test_assert_active_pd_owner_invariant(volume_pos, PD_SLOT_1, PD_SLOT_1, PD_MODE_VOLUME, false);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_1, volume_pos, false));
    key_runtime_integration_scan();
    test_assert_no_pd_owner_invariant();
}

static void test_legacy_volume_preempted_by_pinch_clears_stale_volume_owner(void) {
    keypos_t volume_pos = test_keypos(1, 2);
    keypos_t pinch_pos  = test_keypos(2, 4);

    test_reset_state();
    test_configure_pinch_transparent_profile_path(pinch_pos);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_1, volume_pos, true));
    test_assert_active_pd_owner_invariant(volume_pos, PD_SLOT_1, PD_SLOT_1, PD_MODE_VOLUME, false);

    key_runtime_integration_advance(&fake_time, 10);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
    test_assert_active_pd_owner_invariant(volume_pos, PD_SLOT_1, PD_SLOT_1, PD_MODE_VOLUME, false);
    key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();

    test_assert_active_pd_owner_invariant(pinch_pos, PD_SLOT_5, PD_SLOT_5, PD_MODE_PINCH, true);
    CHECK(noah_runtime_debug_slot_owner_keycode(volume_pos) == PD_SLOT_1);
    CHECK(noah_runtime_debug_slot_held_action_keycode(volume_pos) == KC_NO);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_1, volume_pos, false));
    test_assert_active_pd_owner_invariant(pinch_pos, PD_SLOT_5, PD_SLOT_5, PD_MODE_PINCH, true);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
    key_runtime_integration_scan();
    test_assert_no_pd_owner_invariant();
}

static void test_legacy_pinch_double_tap_salvos_leave_no_pd_owner(void) {
    keypos_t pinch_pos = test_keypos(2, 4);

    test_reset_state();
    test_configure_pinch_transparent_profile_path(pinch_pos);

    for (uint8_t salvo = 0; salvo < 4; salvo++) {
        CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
        test_assert_no_pd_owner_invariant();

        key_runtime_integration_advance(&fake_time, 10);
        CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
        key_runtime_integration_scan();
        test_assert_no_pd_owner_invariant();
        CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 1);

        key_runtime_integration_advance(&fake_time, 20);
        CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
        test_assert_no_pd_owner_invariant();

        key_runtime_integration_advance(&fake_time, 10);
        CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
        key_runtime_integration_scan();
        test_assert_no_pd_owner_invariant();
        CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 1);
        CHECK(delayed_action_count == salvo);

        test_flush_pending_tap_branch();

        CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
        CHECK(noah_runtime_debug_slot_owner_keycode(pinch_pos) == KC_NO);
        CHECK(noah_runtime_debug_slot_held_action_keycode(pinch_pos) == KC_NO);
        CHECK(delayed_action_count == (uint8_t)(salvo + 1u));
        CHECK(last_delayed_action == TEST_PINCH_ZOOM_TAP);

        key_runtime_integration_advance(&fake_time, 40);
    }
}

static void test_legacy_pinch_double_tap_hold_zoom_branch_keeps_single_owner(void) {
    keypos_t pinch_pos = test_keypos(2, 4);

    test_reset_state();
    test_configure_pinch_transparent_profile_path(pinch_pos);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
    test_assert_no_pd_owner_invariant();
    key_runtime_integration_advance(&fake_time, 10);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
    test_assert_no_pd_owner_invariant();

    key_runtime_integration_advance(&fake_time, 20);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
    test_assert_no_pd_owner_invariant();

    key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();
    test_flush_pending_tap_branch();
    test_assert_active_pd_owner_invariant(pinch_pos, PD_SLOT_5, PD_SLOT_3, PD_MODE_ZOOM, false);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
    key_runtime_integration_scan();
    test_assert_no_pd_owner_invariant();
}

static void test_legacy_stacked_pinch_duplicate_press_keeps_owner_token_coherent(void) {
    keypos_t pinch_pos = test_keypos(2, 4);

    test_reset_state();
    test_configure_pinch_transparent_profile_path(pinch_pos);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
    test_assert_no_pd_owner_invariant();

    key_runtime_integration_advance(&fake_time, 5);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
    test_assert_no_pd_owner_invariant();

    key_runtime_integration_advance(&fake_time, 5);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
    key_runtime_integration_scan();
    test_assert_no_pd_owner_invariant();
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 1);

    key_runtime_integration_advance(&fake_time, 20);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, true));
    key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();
    test_flush_pending_tap_branch();
    test_assert_active_pd_owner_invariant(pinch_pos, PD_SLOT_5, PD_SLOT_3, PD_MODE_ZOOM, false);

    CHECK(!key_runtime_integration_process_record(PD_SLOT_5, pinch_pos, false));
    key_runtime_integration_scan();
    test_assert_no_pd_owner_invariant();
}
#endif

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
        CHECK(noah_runtime_debug_slot_held_action_keycode(hold_pos) == PD_SLOT_1);

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

#ifdef NOAH_PD_PROFILE_ENABLE
static void test_last_slot_hold_lock_dpi_and_disabled_slot(void) {
    keypos_t pos = test_keypos(0, 0);
    test_reset_state();
    // The last slot starts disabled: its lock leaves the current one alone.
    CHECK(noah_pd_defaults[7].kind == 0);
    action_dispatch(PD_SLOT_1_LOCK);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    action_dispatch(PD_SLOT_7_LOCK);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    action_dispatch(PD_SLOT_1_LOCK);
    CHECK(pd_mode_local_active_snapshot() == 0);
    noah_pd_config_t last = noah_pd_defaults[1];
    last.id = 7; last.dpi = 1200;
    noah_profile_pd_v1_encode_record(&last, configured_pd_bytes + 8 + 7 * 96);
    publish_configured_pd();
    CHECK(!key_runtime_integration_process_record(PD_SLOT_7, pos, true));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_SLOT_7);
    key_runtime_integration_scan(); // CPI synchronization is scan-owned.
    CHECK(current_cpi == 1200);
    CHECK(!key_runtime_integration_process_record(PD_SLOT_7, pos, false));
    key_runtime_integration_scan();
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(current_cpi == default_dpi);
    action_dispatch(PD_SLOT_7_LOCK);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_SLOT_7);
    action_dispatch(PD_SLOT_7_LOCK);
    CHECK(pd_mode_local_locked_snapshot() == 0);
}
#endif

// ── Mouse buttons a pointing mode consumes ─────────────────────────────────
//
// Arrow overrides all three buttons: button 1 holds Shift, 2 taps copy and 3
// taps paste. A consumed press never reaches the button's own behavior, so
// after the release nothing the behavior would have started may remain.

static void test_lock_arrow(void) {
    action_dispatch(PD_SLOT_4_LOCK);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
}

static void test_assert_button_quiescent(keypos_t pos) {
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(pos) == KC_NO);
    CHECK(!noah_runtime_debug_slot_has_pending_multi_tap(pos));
    CHECK(!key_runtime_core_repeat_active_at(pos));
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));
    CHECK(fake_mods == 0 && fake_managed_mods == 0);
    CHECK(!key_runtime_core_take_intercepted_release(pos));
#ifdef NOAH_PD_PROFILE_ENABLE
    CHECK(!noah_pd_engine_pending_release());
#endif
}

// While consumed, a held button holds nothing of its own at any point.
static void test_hold_consumed_button(uint16_t button, keypos_t pos, uint16_t held_ms) {
    for (uint16_t elapsed = 0; elapsed < held_ms; elapsed += 20u) {
        key_runtime_integration_advance(&fake_time, 20u);
        key_runtime_integration_scan();
        CHECK(noah_runtime_debug_slot_held_action_keycode(pos) == KC_NO);
        CHECK(!key_runtime_core_repeat_active_at(pos));
        CHECK((fake_mods & MOD_BIT(KC_LEFT_ALT)) == 0);
    }
    (void)button;
}

static void test_release_and_settle(uint16_t button, keypos_t pos) {
    CHECK(!key_runtime_integration_process_record(button, pos, false));
    key_runtime_integration_advance(&fake_time, 1000u);
    key_runtime_integration_scan();
}

static void test_factory_arrow_paste_button_never_holds_itself(void) {
    const uint16_t durations[] = {40u, 101u, 1000u};

    for (uint8_t index = 0; index < ARRAY_SIZE(durations); index++) {
        keypos_t pos = test_keypos(4, 2);

        test_reset_state();
        test_lock_arrow();
        CHECK(!key_runtime_integration_process_record(MS_BTN3, pos, true));
        CHECK(tap_code16_count == 1u && last_tap_code16 == G(KC_V));
        test_hold_consumed_button(MS_BTN3, pos, durations[index]);
        test_release_and_settle(MS_BTN3, pos);
        CHECK(tap_code16_count == 1u);
        test_assert_button_quiescent(pos);
    }
}

// A tap/hold and a repeating behavior on buttons Arrow consumes: neither the
// tap, the hold, nor the repeat runs, and each button's override still does.
static void test_consumed_buttons_run_no_authored_behavior(void) {
    keypos_t shift_pos = test_keypos(4, 0);
    keypos_t copy_pos  = test_keypos(4, 1);

    test_reset_state();
    test_lock_arrow();
    CHECK(!key_runtime_integration_process_record(MS_BTN1, shift_pos, true));
#ifdef NOAH_PD_PROFILE_ENABLE
    CHECK((fake_mods & MOD_BIT(KC_RIGHT_SHIFT)) != 0);
#endif
    test_hold_consumed_button(MS_BTN1, shift_pos, TEST_PD_TAP_HOLD_TERM + 40u);
    test_release_and_settle(MS_BTN1, shift_pos);
    CHECK(tap_code16_count == 0u);
    test_assert_button_quiescent(shift_pos);

    CHECK(!key_runtime_integration_process_record(MS_BTN2, copy_pos, true));
    CHECK(tap_code16_count == 1u && last_tap_code16 == G(KC_C));
    test_hold_consumed_button(MS_BTN2, copy_pos, TEST_PD_TAP_HOLD_TERM + 200u);
    test_release_and_settle(MS_BTN2, copy_pos);
    CHECK(tap_code16_count == 1u);
    test_assert_button_quiescent(copy_pos);

    // Short presses resolve the same way as long ones.
    CHECK(!key_runtime_integration_process_record(MS_BTN1, shift_pos, true));
    test_release_and_settle(MS_BTN1, shift_pos);
    CHECK(tap_code16_count == 1u);
    test_assert_button_quiescent(shift_pos);
}

// A release goes to whoever took the press, across a change of mode.
static void test_button_release_follows_its_press_across_mode_changes(void) {
    keypos_t pos = test_keypos(4, 0);

    // Consumed by Arrow, released after Arrow was unlocked: the mode's Shift
    // ends, and the button's own behavior still never runs.
    test_reset_state();
    test_lock_arrow();
    CHECK(!key_runtime_integration_process_record(MS_BTN1, pos, true));
    action_dispatch(PD_SLOT_4_LOCK);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    test_hold_consumed_button(MS_BTN1, pos, TEST_PD_TAP_HOLD_TERM + 40u);
    test_release_and_settle(MS_BTN1, pos);
    CHECK(tap_code16_count == 0u);
    test_assert_button_quiescent(pos);

    // Pressed before Arrow, released under it: the behavior took the press,
    // so its Alt hold ends on the release instead of the mode swallowing it.
    test_reset_state();
    CHECK(!key_runtime_integration_process_record(MS_BTN1, pos, true));
    key_runtime_integration_advance(&fake_time, TEST_PD_TAP_HOLD_TERM + 1u);
    key_runtime_integration_scan();
    CHECK(noah_runtime_debug_slot_held_action_keycode(pos) == KC_LEFT_ALT);
    test_lock_arrow();
    test_release_and_settle(MS_BTN1, pos);
    test_assert_button_quiescent(pos);
    action_dispatch(PD_SLOT_4_LOCK);
    CHECK(pd_mode_local_active_snapshot() == 0 && pd_mode_local_locked_snapshot() == 0);
}

// Another key pressed while a consumed button is held behaves as usual.
static void test_consumed_button_is_an_ordinary_interrupting_press(void) {
    keypos_t button_pos = test_keypos(4, 2);
    keypos_t key_pos    = test_keypos(1, 1);

    test_reset_state();
    test_lock_arrow();
    CHECK(!key_runtime_integration_process_record(MS_BTN3, button_pos, true));
    CHECK(!key_runtime_integration_process_record(TEST_HANDLED_TAP_KEY, key_pos, true));
    CHECK(!key_runtime_integration_process_record(TEST_HANDLED_TAP_KEY, key_pos, false));
    test_hold_consumed_button(MS_BTN3, button_pos, 200u);
    CHECK(tap_code16_count == 2u && last_tap_code16 == KC_J);
    test_release_and_settle(MS_BTN3, button_pos);
    test_assert_button_quiescent(button_pos);
    test_assert_button_quiescent(key_pos);

    // Unlocked with nothing held, the buttons are ordinary again.
    action_dispatch(PD_SLOT_4_LOCK);
    CHECK(pd_mode_local_active_snapshot() == 0 && pd_mode_local_locked_snapshot() == 0);
    CHECK(!key_runtime_integration_process_record(MS_BTN1, button_pos, true));
    CHECK(!key_runtime_integration_process_record(MS_BTN1, button_pos, false));
    key_runtime_integration_advance(&fake_time, TEST_PD_MULTI_TAP_TERM + 1u);
    key_runtime_integration_scan();
    CHECK(last_tap_code16 == KC_A);
}

#ifdef NOAH_PD_PROFILE_ENABLE
// A slot may simply consume a button; its behavior must not run either.
static void test_configured_consume_button_runs_nothing(void) {
    keypos_t pos = test_keypos(4, 0);

    test_reset_state();
    configured_pd_bytes[8 + 96 + 52] = 1u; // Volume consumes button 1.
    publish_configured_pd();
    action_dispatch(PD_SLOT_1_LOCK);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    CHECK(!key_runtime_integration_process_record(MS_BTN1, pos, true));
    test_hold_consumed_button(MS_BTN1, pos, TEST_PD_TAP_HOLD_TERM + 40u);
    test_release_and_settle(MS_BTN1, pos);
    CHECK(tap_code16_count == 0u);
    test_assert_button_quiescent(pos);
}
#endif

// The sparse row above matches a live row with only double-hold authored.
// Both the first press duration and the released gap must fit their terms.
static void test_ordinary_mouse_button_double_tap_hold(void) {
    const struct {
        uint16_t first_press_ms;
        uint16_t gap_ms;
        uint16_t expected_hold;
    } cases[] = {
        {40u, 40u, MS_BTN7},
        {99u, 99u, MS_BTN7},
        {101u, 40u, MS_BTN3},
        {40u, 101u, MS_BTN3},
    };
    keypos_t pos = test_keypos(4, 2);

    for (uint8_t index = 0; index < ARRAY_SIZE(cases); index++) {
        test_reset_state();
        CHECK(!key_runtime_integration_process_record(MS_BTN3, pos, true));
        key_runtime_integration_advance(&fake_time, cases[index].first_press_ms);
        key_runtime_integration_scan();
        CHECK(!key_runtime_integration_process_record(MS_BTN3, pos, false));
        key_runtime_integration_advance(&fake_time, cases[index].gap_ms);
        key_runtime_integration_scan();
        CHECK(!key_runtime_integration_process_record(MS_BTN3, pos, true));
        key_runtime_integration_advance(&fake_time, 101u);
        key_runtime_integration_scan();
        CHECK(noah_runtime_debug_slot_held_action_keycode(pos) == cases[index].expected_hold);
        test_release_and_settle(MS_BTN3, pos);
        test_assert_button_quiescent(pos);
    }
}

#ifdef NOAH_TEST_QMK_GESTURES
bool combo_key_event_pending(uint8_t row, uint8_t col, bool pressed, uint16_t since, uint16_t term);
void gesture_engine_event(uint8_t row, uint8_t col, bool down, uint16_t time);
void gesture_engine_scan(uint16_t time);
const uint8_t noah_combo_count = 10;
combo_t key_combos[10];
const uint16_t *gesture_engine_combo_keys(uint16_t index);
uint16_t gesture_engine_combo_output(uint16_t index);
bool gesture_engine_combo_active(uint16_t index);
bool gesture_engine_combo_disabled(uint16_t index);
static void gesture_sync_combos(void) {
    for (uint16_t i = 0; i < noah_combo_count; i++) {
        key_combos[i] = (combo_t){.keys = gesture_engine_combo_keys(i), .keycode = gesture_engine_combo_output(i),
            .active = gesture_engine_combo_active(i), .disabled = gesture_engine_combo_disabled(i)};
    }
}
static uint16_t gesture_test_code;
static uint16_t gesture_delivered_press;
uint16_t gesture_keycode(uint8_t row, uint8_t col) {
    if (row == 3 && col == 0) return gesture_test_code;
    static const uint16_t keys[2][8] = {
        {PD_SLOT_5, MS_BTN1, MS_BTN3, LT(3,KC_SLSH), LT(2,KC_A), KC_COMM, PD_SLOT_0, MS_BTN2},
        {KC_M, KC_DOT, KC_N, G(KC_C), G(KC_V), PD_SLOT_1, KC_D, LT(3,KC_F)},
    };
    return row >= 4 && row < 6 && col < 8 ? keys[row-4][col] : KC_NO;
}
static uint16_t gesture_combo_outputs[32];
static uint8_t gesture_combo_output_count;
void gesture_deliver(uint16_t code, uint8_t row, uint8_t col, bool down, uint16_t time, uint8_t taps, bool combo) {
    gesture_sync_combos();
    if (down) gesture_delivered_press = code;
    if (combo && down) { CHECK(gesture_combo_output_count < ARRAY_SIZE(gesture_combo_outputs)); gesture_combo_outputs[gesture_combo_output_count++] = code; }
    keyrecord_t r = {.event = {.key={row,col}, .pressed=down, .type=combo ? COMBO_EVENT : KEY_EVENT, .time=time}, .tap={.count=taps}};
    bool pass = noah_process_record_user(code, &r);

    noah_process_record_user_finalize(code, &r, pass);
}
static void gesture_advance(uint16_t ms) {
    // Mirrors noah_matrix_scan_user: combo-origin housekeeping, then key runtime.
    while (ms--) { fake_time++; gesture_sync_combos(); noah_qmk_combo_origin_scan(); key_runtime_integration_scan(); gesture_engine_scan(fake_time); }
}
static void gesture_at(uint8_t row, uint8_t col, bool down) {
    gesture_sync_combos();
    keyrecord_t r = {.event={.key={row,col}, .pressed=down, .type=KEY_EVENT, .time=fake_time}};
    CHECK(noah_pre_process_record_user(gesture_keycode(row,col), &r));
    gesture_engine_event(row,col,down,fake_time);
}
static void gesture_event(bool down) { gesture_at(4,2,down); }
static void test_qmk_buffered_second_press(void) {
    const uint16_t gaps[] = {0, 40, 70, 99, 100, 101};
    const uint16_t starts[] = {1000, 65500};
    for (uint8_t start = 0; start < ARRAY_SIZE(starts); start++) {
        for (uint8_t gap = 0; gap < ARRAY_SIZE(gaps); gap++) {
            test_reset_state(); fake_time = starts[start];
            gesture_event(true); gesture_advance(40); gesture_event(false);
            gesture_advance(gaps[gap]); gesture_event(true);
            gesture_advance(99);
            CHECK(noah_runtime_debug_slot_held_action_keycode(test_keypos(4,2)) == KC_NO);
            gesture_advance(1);
            CHECK(noah_runtime_debug_slot_held_action_keycode(test_keypos(4,2)) == (gaps[gap] <= 100 ? MS_BTN7 : MS_BTN3));
            gesture_event(false); gesture_advance(500);
            test_assert_button_quiescent(test_keypos(4,2));
        }
    }
}
// A chord winning after an eligible second press consumes the member. Its old
// single-tap candidate must settle, not remain reserved after suppression.
static void test_qmk_combo_consumes_second_press(void) {
    test_reset_state(); fake_time = 2000;
    gesture_event(true); gesture_advance(40); gesture_event(false);
    gesture_advance(70); gesture_event(true);
    gesture_advance(20); gesture_at(4,0,true);
    gesture_advance(120);
    CHECK(noah_runtime_debug_slot_held_action_keycode(test_keypos(4,2)) != MS_BTN7);
    gesture_event(false); gesture_at(4,0,false); gesture_advance(500);
    test_assert_button_quiescent(test_keypos(4,2));
}
static void test_qmk_member_hold_uses_physical_duration(void) {
    test_reset_state(); fake_time = 3000;
    gesture_event(true); gesture_advance(99);
    CHECK(noah_runtime_debug_slot_held_action_keycode(test_keypos(4,2)) == KC_NO);
    gesture_advance(1);
    CHECK(noah_runtime_debug_slot_held_action_keycode(test_keypos(4,2)) == MS_BTN3);
    gesture_event(false); gesture_advance(500);
    test_assert_button_quiescent(test_keypos(4,2));
}
static void test_qmk_authored_layer_tap_has_one_clock(void) {
    test_reset_state(); fake_time = 4000; gesture_locks = 0;
    gesture_at(4,3,true); gesture_advance(40); gesture_at(4,3,false);
    gesture_advance(70); gesture_at(4,3,true);
    // The 100 ms chord window must not be followed by QMK's 200 ms
    // tapping window before the authored 100 ms double hold can lock Nav.
    gesture_advance(102);
    CHECK(layer_state_cmp(gesture_locks, TEST_LAYER_NAV));
    gesture_at(4,3,false); gesture_advance(500);
    CHECK(layer_state_cmp(gesture_locks, TEST_LAYER_NAV));
}
static void test_qmk_tapping_queue_preserves_member_series(void) {
    test_reset_state(); fake_time = 5000;
    gesture_event(true); gesture_advance(40); gesture_event(false);
    gesture_advance(10); gesture_at(4,4,true); // plain native LT
    gesture_advance(60); gesture_event(true);
    gesture_advance(200);
    CHECK(noah_runtime_debug_slot_held_action_keycode(test_keypos(4,2)) == MS_BTN7);
    gesture_event(false); gesture_at(4,4,false); gesture_advance(500);
    test_assert_button_quiescent(test_keypos(4,2));
}
// QMK passes most releases straight through a pending tapping key, but holds
// back a modifier's release until that key resolves. Left GUI is delivered at
// once, released physically at 50 ms, and its release waits behind a native LT
// for 200 ms; its 150 ms fallback hold must not fire from delayed delivery.
static void test_qmk_queued_release_cannot_become_hold(void) {
    test_reset_state(); fake_time = 6000; gesture_test_code = KC_LEFT_GUI;
    gesture_at(3,0,true); gesture_advance(20);
    gesture_at(4,4,true); gesture_advance(30); // native LT becomes QMK's tapping key
    gesture_at(3,0,false); // physical release at 50 ms, queued behind LT
    for (unsigned i=0; i<200; i++) {
        gesture_advance(1);
        CHECK((fake_mods & MOD_BIT(KC_LEFT_GUI)) == 0);
        CHECK(noah_runtime_debug_slot_held_action_keycode(test_keypos(3,0)) == KC_NO);
    }
    gesture_at(4,4,false); gesture_advance(500);
    CHECK((fake_mods & MOD_BIT(KC_LEFT_GUI)) == 0);
    test_assert_button_quiescent(test_keypos(3,0));
}
// Screenshot C5: Button 1 + Volume -> GUI; GUI double hold -> Alt,
// triple tap -> OSM(Shift), inherited hold/repeat terms both 150 ms.
static void gesture_gui_chord(bool down, bool reversed) {
    const keypos_t keys[] = {{4, 1}, {5, 5}};
    keypos_t first = keys[reversed ? 1 : 0], second = keys[reversed ? 0 : 1];
    gesture_at(first.row, first.col, down);
    gesture_advance(10);
    gesture_at(second.row, second.col, down);
}
static void test_qmk_combo_output_gui_behaviour(void) {
    for (unsigned press_order = 0; press_order < 2; press_order++) {
        for (unsigned release_order = 0; release_order < 2; release_order++) {
            test_reset_state(); fake_time = 12000; gesture_combo_output_count = 0;
            gesture_gui_chord(true, press_order); gesture_advance(320);
            CHECK(gesture_combo_output_count == 1 && gesture_combo_outputs[0] == KC_LEFT_GUI);
            CHECK((fake_mods & MOD_BIT(KC_LEFT_GUI)) != 0);
            CHECK((fake_mods & MOD_BIT(KC_LEFT_ALT)) == 0);
            CHECK(pd_mode_local_active_snapshot() == 0);
            gesture_gui_chord(false, release_order); gesture_advance(500);
            test_assert_button_quiescent(test_keypos(0, 0));

            test_reset_state(); fake_time = 14000; gesture_combo_output_count = 0;
            gesture_gui_chord(true, press_order); gesture_advance(20);
            gesture_gui_chord(false, release_order); gesture_advance(60);
            gesture_gui_chord(true, !press_order); gesture_advance(320);
            CHECK(gesture_combo_output_count == 2);
            CHECK((fake_mods & MOD_BIT(KC_LEFT_ALT)) != 0);
            CHECK((fake_mods & MOD_BIT(KC_LEFT_GUI)) == 0);
            CHECK(pd_mode_local_active_snapshot() == 0);
            gesture_gui_chord(false, !release_order); gesture_advance(500);
            test_assert_button_quiescent(test_keypos(0, 0));

            test_reset_state(); fake_time = 16000; gesture_combo_output_count = 0;
            gesture_qmk_tap_count = 0; gesture_qmk_tap = KC_NO;
            for (unsigned tap = 0; tap < 3; tap++) {
                gesture_gui_chord(true, tap % 2 ? !press_order : press_order);
                gesture_advance(20);
                CHECK(gesture_qmk_tap_count == 0); // Never send Shift on chord press.
                gesture_gui_chord(false, release_order);
                gesture_advance(tap < 2 ? 60 : 500);
            }
            CHECK(gesture_combo_output_count == 3);
            CHECK(gesture_qmk_tap_count == 1 && gesture_qmk_tap == OSM(MOD_LSFT));
            CHECK(pd_mode_local_active_snapshot() == 0);
            test_assert_button_quiescent(test_keypos(0, 0));
        }
    }
}

// A row that authors only a repeated tap keeps the key's own first tap and
// hold: MT/OSM hold their modifiers past the threshold, as native QMK does,
// and a hold sends no tap or one-shot on release.
static void test_qmk_sparse_dual_role_rows_keep_intrinsic_hold(void) {
    const struct { uint16_t code; uint16_t mods; } rows[] = {
        {MT(MOD_LSFT | MOD_LGUI, KC_S), LSG(KC_NO)},
        {OSM(MOD_LALT), LALT(KC_NO)},
    };
    for (unsigned i = 0; i < ARRAY_SIZE(rows); i++) {
        test_reset_state(); fake_time = 20000; gesture_test_code = rows[i].code;
        gesture_registered = gesture_unregistered = KC_NO; gesture_qmk_tap_count = 0;
        gesture_at(3, 0, true); gesture_advance(99);
        CHECK(gesture_registered == KC_NO);
        gesture_advance(1);
        CHECK(gesture_registered == rows[i].mods);
        gesture_advance(150); gesture_at(3, 0, false);
        CHECK(gesture_unregistered == rows[i].mods);
        gesture_advance(400);
        CHECK(gesture_qmk_tap_count == 0);
        test_assert_button_quiescent(test_keypos(3, 0));

        test_reset_state(); fake_time = 22000; gesture_test_code = rows[i].code;
        gesture_registered = KC_NO; gesture_qmk_tap_count = 0; gesture_qmk_tap = KC_NO;
        gesture_at(3, 0, true); gesture_advance(40); gesture_at(3, 0, false); gesture_advance(400);
        CHECK(gesture_registered == KC_NO);
        CHECK(gesture_qmk_tap_count == 1 && gesture_qmk_tap == rows[i].code);
    }
}

static void test_qmk_dual_role_ownership(void) {
    const uint16_t authored[] = {MT(MOD_LCTL, KC_A), TT(2), OSL(2), OSM(MOD_LSFT)};
    for (unsigned i = 0; i < ARRAY_SIZE(authored); i++) {
        test_reset_state(); fake_time = 9000; gesture_test_code = authored[i]; gesture_delivered_press = KC_NO;
        gesture_at(3, 0, true);
        CHECK(gesture_delivered_press == authored[i]);
        gesture_advance(99);
        CHECK(noah_runtime_debug_slot_held_action_keycode(test_keypos(3, 0)) == KC_NO);
        gesture_advance(1);
        CHECK(noah_runtime_debug_slot_held_action_keycode(test_keypos(3, 0)) == MS_BTN7);
        gesture_at(3, 0, false); gesture_advance(500);
        test_assert_button_quiescent(test_keypos(3, 0));
    }
    const uint16_t owned[] = {TT(3), OSL(3)};
    for (unsigned i = 0; i < ARRAY_SIZE(owned); i++) {
        test_reset_state(); fake_time = 10000; gesture_test_code = owned[i]; gesture_delivered_press = KC_NO; gesture_momentary_layer = UINT8_MAX;
        gesture_at(3, 0, true);
        CHECK(gesture_delivered_press == owned[i]);
        CHECK(gesture_momentary_layer == 3);
        gesture_advance(250); gesture_at(3, 0, false); gesture_advance(500);
    }
    const uint16_t native[] = {LT(2, KC_A), MT(MOD_LCTL, KC_B), OSM(MOD_RSFT)};
    for (unsigned i = 0; i < ARRAY_SIZE(native); i++) {
        test_reset_state(); fake_time = 11000; gesture_test_code = native[i]; gesture_delivered_press = KC_NO;
        gesture_at(3, 0, true); gesture_advance(100);
        CHECK(gesture_delivered_press != native[i]);
        gesture_advance(150);
        CHECK(gesture_delivered_press == native[i]);
        gesture_at(3, 0, false); gesture_advance(500);
    }
}

static void test_qmk_nested_chords_choose_only_largest(void) {
    const keypos_t families[2][4] = {{{4,1},{4,7},{4,6},{4,3}}, {{5,0},{4,5},{5,1},{4,3}}};
    for (unsigned family=0; family<2; family++) {
        for (unsigned a=0; a<4; a++) for (unsigned b=0; b<4; b++) {
            if (a==b) continue;
            for (unsigned c=0; c<4; c++) {
                if (c==a || c==b) continue;
                unsigned d=6-a-b-c;
                const unsigned order[]={a,b,c,d};
                test_reset_state(); fake_time=7000; gesture_combo_output_count=0;
                for (unsigned i=0;i<4;i++) { keypos_t pos=families[family][order[i]]; gesture_at(pos.row,pos.col,true); gesture_advance(10); }
                gesture_advance(120);
                CHECK(gesture_combo_output_count==1);
                CHECK(gesture_combo_outputs[0]==G(KC_N));
                for (unsigned i=0;i<4;i++) { keypos_t pos=families[family][order[3-i]]; gesture_at(pos.row,pos.col,false); gesture_advance(10); }
                gesture_advance(500);
                CHECK(noah_runtime_debug_active_slot_count()==0);
                CHECK(noah_runtime_debug_pending_multi_tap_slot_count()==0);
            }
        }
    }
}
#endif

int main(void) {
#ifdef NOAH_TEST_QMK_GESTURES
    test_qmk_buffered_second_press();
    test_qmk_combo_consumes_second_press();
    test_qmk_member_hold_uses_physical_duration();
    test_qmk_authored_layer_tap_has_one_clock();
    test_qmk_tapping_queue_preserves_member_series();
    test_qmk_queued_release_cannot_become_hold();
    test_qmk_nested_chords_choose_only_largest();
    test_qmk_dual_role_ownership();
    test_qmk_sparse_dual_role_rows_keep_intrinsic_hold();
    test_qmk_combo_output_gui_behaviour();
    puts("QMK gesture pipeline tests passed");
    return 0;
#endif
    test_ordinary_mouse_button_double_tap_hold();
    test_factory_arrow_paste_button_never_holds_itself();
    test_consumed_buttons_run_no_authored_behavior();
    test_button_release_follows_its_press_across_mode_changes();
    test_consumed_button_is_an_ordinary_interrupting_press();
#ifdef NOAH_PD_PROFILE_ENABLE
    test_configured_consume_button_runs_nothing();
#endif
#ifdef NOAH_PD_PROFILE_ENABLE
    test_last_slot_hold_lock_dpi_and_disabled_slot();
#endif
    test_authored_single_press_preserves_default_pd_mode_hold();
    test_authored_single_press_pd_mode_hold_dispatches_plain_taps_immediately();
    test_raw_lt_hold_dispatches_authored_tap_key_immediately();
    test_raw_lt_hold_dispatches_authored_plain_tap_immediately();
    test_authored_layer_hold_dispatches_authored_tap_key_immediately();
    test_authored_layer_hold_dispatches_authored_plain_tap_immediately();
    test_interrupted_locked_pd_mode_press_unlocks_on_press_and_releases_momentary_hold();
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
#if defined(PD_MODE_KEY_RUNTIME_TEST_LEGACY_PINCH_IMPLICIT)
    test_legacy_pinch_preempted_by_volume_clears_stale_pinch_owner();
    test_legacy_volume_preempted_by_pinch_clears_stale_volume_owner();
    test_legacy_pinch_double_tap_salvos_leave_no_pd_owner();
    test_legacy_pinch_double_tap_hold_zoom_branch_keeps_single_owner();
    test_legacy_stacked_pinch_duplicate_press_keeps_owner_token_coherent();
#else
    test_pinch_single_tap_defers_mode_owned_gui_from_delayed_replay();
    test_pinch_single_tap_preserves_physically_held_gui_on_delayed_replay();
    test_pinch_masks_mode_owned_gui_during_concurrent_plain_key_processing();
    test_pinch_keeps_physically_held_gui_visible_during_concurrent_plain_key_processing();
    test_pinch_double_tap_hold_zoom_branch_keeps_projection_coherent();
    test_volume_pinch_zoom_volume_alternation_keeps_projection_coherent();
    test_pinch_double_tap_salvos_stay_quiescent();
#endif
    test_gui_double_tap_hold_with_authored_pd_hold_keeps_processed_child_immediate();

    puts("pd_mode_key_runtime_integration host tests passed");
    return 0;
}
