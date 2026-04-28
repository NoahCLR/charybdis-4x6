#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "key_runtime_integration_harness.h"
#include "print.h"
#include "transactions.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/lib/compat/qmk_combo_origin.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/key/runtime/feedback.h"
#include "users/noah/lib/key/runtime/slot/origin_registry.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/key/runtime/projection/projection.h"
#include "users/noah/lib/key/runtime/reducer/runtime.h"
#include "users/noah/lib/key/runtime/trace/core_trace.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "users/noah/lib/state/ownership/layer_ownership.h"
#include "users/noah/lib/state/diagnostics/runtime_debug.h"
#include "users/noah/lib/state/shared/runtime_reset.h"
#include "users/noah/lib/state/diagnostics/runtime_trace.h"
#include "users/noah/noah_keymap.h"
#include "users/noah/noah_runtime.h"

layer_state_t layer_state = 0;

#define TEST_DELAYED_ACTION_LOG_CAPACITY 8u

static uint16_t fake_time;
static uint16_t test_tap_code16_count;
static uint16_t test_last_tap_code16;
static uint16_t test_delayed_action_count;
static uint16_t test_last_delayed_action;
static uint16_t test_delayed_actions[TEST_DELAYED_ACTION_LOG_CAPACITY];
static uint16_t test_pressed_keycodes[MATRIX_ROWS][MATRIX_COLS];
static uint16_t current_cpi;
static uint8_t  fake_mods;
static uint8_t  fake_weak_mods;
static uint8_t  fake_oneshot_mods;
static uint8_t  fake_oneshot_locked_mods;
static uint8_t  fake_managed_mods;
static uint8_t  fake_physical_mods;
static bool     dragscroll_enabled;
static bool     sniping_enabled;
static bool     auto_mouse_enabled;
static bool     auto_mouse_toggled;
static int8_t   auto_mouse_key_tracker;
static uint8_t  auto_mouse_layer_target;
static uint8_t  auto_mouse_layer_off_count;
static uint8_t  reset_dragscroll_count;

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
} test_held_action_binding_t;

static test_held_action_binding_t test_held_actions[MATRIX_ROWS * MATRIX_COLS];

extern const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS];

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

static keypos_t test_left_thumb_pos(void) {
    return (keypos_t){.row = 6, .col = 2};
}

static keypos_t test_right_thumb_pos(void) {
    return (keypos_t){.row = 6, .col = 3};
}

static bool test_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static uint8_t test_modifier_mask(uint16_t keycode) {
    switch (keycode) {
        case KC_LEFT_GUI:
            return MOD_BIT(KC_LEFT_GUI);
        case KC_LEFT_ALT:
            return MOD_BIT(KC_LEFT_ALT);
        case KC_RIGHT_ALT:
            return MOD_BIT(KC_RIGHT_ALT);
        default:
            return 0;
    }
}

static uint16_t test_layer_mask(uint8_t layer) {
    return (uint16_t)((layer_state_t)1u << layer);
}

static bool test_layer_active(uint8_t layer) {
    return layer < LAYER_COUNT && (layer_state & ((layer_state_t)1u << layer)) != 0;
}

static bool test_layer_locked(uint8_t layer) {
    return layer < LAYER_COUNT && layer_ownership_is_locked(layer);
}

static uint8_t test_highest_active_layer(void) {
    for (int8_t layer = (int8_t)LAYER_COUNT - 1; layer >= 0; layer--) {
        if ((layer_state & ((layer_state_t)1u << layer)) != 0) {
            return (uint8_t)layer;
        }
    }

    return LAYER_BASE;
}

static uint16_t test_keycode_at(uint8_t layer_num, keypos_t key_pos) {
    return keymaps[layer_num][key_pos.row][key_pos.col];
}

static bool test_keypos_valid(keypos_t key_pos) {
    return key_pos.row < MATRIX_ROWS && key_pos.col < MATRIX_COLS;
}

static keypos_t test_find_keypos_on_layer(uint8_t layer_num, uint16_t keycode) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t key_pos = {
                .row = row,
                .col = col,
            };

            if (test_keycode_at(layer_num, key_pos) == keycode) {
                return key_pos;
            }
        }
    }

    return (keypos_t){
        .row = UINT8_MAX,
        .col = UINT8_MAX,
    };
}

static int16_t test_find_held_action_slot(keypos_t key_pos) {
    for (uint16_t index = 0; index < ARRAY_SIZE(test_held_actions); index++) {
        if (test_held_actions[index].active && test_keypos_equal(test_held_actions[index].key_pos, key_pos)) {
            return (int16_t)index;
        }
    }

    return -1;
}

static int16_t test_find_free_held_action_slot(void) {
    for (uint16_t index = 0; index < ARRAY_SIZE(test_held_actions); index++) {
        if (!test_held_actions[index].active) {
            return (int16_t)index;
        }
    }

    return -1;
}

static uint16_t test_resolve_keycode(keypos_t key_pos) {
    uint8_t highest = test_highest_active_layer();

    for (int8_t layer = (int8_t)highest; layer >= 0; layer--) {
        uint16_t keycode = test_keycode_at((uint8_t)layer, key_pos);
        if (keycode != KC_TRNS) {
            return keycode;
        }
    }

    return KC_NO;
}

static uint16_t test_cached_press_keycode(keypos_t key_pos) {
    if (!test_keypos_valid(key_pos)) {
        return KC_NO;
    }

    return test_pressed_keycodes[key_pos.row][key_pos.col];
}

static void test_cache_press_keycode(keypos_t key_pos, uint16_t keycode) {
    if (!test_keypos_valid(key_pos)) {
        return;
    }

    test_pressed_keycodes[key_pos.row][key_pos.col] = keycode;
}

static void test_clear_press_keycode(keypos_t key_pos) {
    if (!test_keypos_valid(key_pos)) {
        return;
    }

    test_pressed_keycodes[key_pos.row][key_pos.col] = KC_NO;
}

static void test_press_resolved(keypos_t key_pos) {
    uint16_t keycode = test_resolve_keycode(key_pos);

    test_cache_press_keycode(key_pos, keycode);
    CHECK(!key_runtime_integration_process_record(keycode, key_pos, true));
}

static void test_release_resolved(keypos_t key_pos) {
    uint16_t keycode = test_cached_press_keycode(key_pos);

    if (keycode == KC_NO) {
        keycode = test_resolve_keycode(key_pos);
    }

    CHECK(!key_runtime_integration_process_record(keycode, key_pos, false));
    test_clear_press_keycode(key_pos);
}

static int16_t test_find_combo_index_for_exact_keys(const uint16_t *keys, uint8_t key_count) {
    if (!(keys && key_count != 0u)) {
        return -1;
    }

    for (uint8_t combo_index = 0; combo_index < noah_combo_count; combo_index++) {
        const combo_t *combo = &key_combos[combo_index];
        bool           match = true;

        for (uint8_t member_index = 0; member_index < key_count; member_index++) {
            if (combo->keys[member_index] != keys[member_index]) {
                match = false;
                break;
            }
        }

        if (!(match && combo->keys[key_count] == COMBO_END)) {
            continue;
        }

        return (int16_t)combo_index;
    }

    return -1;
}

static void test_observe_combo_member(keypos_t key_pos, bool pressed) {
    keyrecord_t record = {
        .event =
            {
                .type    = KEY_EVENT,
                .key     = key_pos,
                .pressed = pressed,
            },
    };

    noah_qmk_combo_origin_observe_physical_key_event(test_resolve_keycode(key_pos), &record);
}

static bool test_process_combo_output(uint16_t keycode, bool pressed) {
    keyrecord_t record = {
        .event = MAKE_COMBOEVENT(pressed),
    };
    bool keep_processing;

    if (!noah_pre_process_record_user(keycode, &record)) {
        return false;
    }

    keep_processing = noah_process_record_user(keycode, &record);
    if (!keep_processing) {
        noah_process_record_user_finalize(keycode, &record, false);
        return false;
    }

    noah_post_process_record_user(keycode, &record);
    return true;
}

static void test_run_double_tap_hold_cycle(keypos_t key_pos, uint16_t hold_ms) {
    test_press_resolved(key_pos);
    test_release_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, 40);
    test_press_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, hold_ms);
    key_runtime_integration_scan();

    test_release_resolved(key_pos);
}

static void test_run_double_tap_hold_cycle_with_intermediate_scan(keypos_t key_pos, uint16_t pre_threshold_scan_ms, uint16_t hold_ms) {
    test_press_resolved(key_pos);
    test_release_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, 40);
    test_press_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, pre_threshold_scan_ms);
    key_runtime_integration_scan();

    key_runtime_integration_advance(&fake_time, hold_ms);
    key_runtime_integration_scan();

    test_release_resolved(key_pos);
}

static void test_run_quick_tap(keypos_t key_pos) {
    test_press_resolved(key_pos);
    test_release_resolved(key_pos);
}

static void test_advance_thumb_multi_tap_gap(void) {
    key_runtime_integration_advance(&fake_time, 40);
}

static void test_finish_tap_branch_confirmation(void) {
    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_BRANCH_CONFIRM_TERM + 1);
    key_runtime_integration_scan();
}

static void test_assert_thumb_runtime_quiescent(keypos_t key_pos) {
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_pending_multi_tap_count(key_pos) == 0);
    CHECK(!noah_runtime_debug_slot_pending_multi_tap_holding(key_pos));
}

static key_feedback_semantic_t test_feedback_semantic_for_key(keypos_t key_pos) {
    uint8_t semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];

    key_feedback_semantic_map(semantic_map);
    return key_feedback_semantic_map_get(semantic_map, key_pos);
}

static uint8_t test_feedback_tap_branch_for_key(keypos_t key_pos) {
    uint8_t tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];

    key_feedback_tap_branch_map(tap_branch_map);
    return key_feedback_tap_branch_map_get(tap_branch_map, key_pos);
}

static void test_assert_no_tap_branch_feedback(keypos_t key_pos) {
    key_feedback_semantic_t semantic = test_feedback_semantic_for_key(key_pos);

    CHECK(semantic != KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    CHECK(semantic != KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 0u);
}

key_feedback_tap_commit_mode_t key_feedback_tap_commit_mode(void) {
    return KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS;
}

typedef enum {
    TEST_RELEASE_TARGET_CHILD = 0,
    TEST_RELEASE_TARGET_PARENT,
    TEST_RELEASE_TARGET_GUI,
} test_release_target_t;

typedef struct {
    const char           *name;
    test_release_target_t order[3];
} test_release_order_case_t;

static const test_release_order_case_t test_release_orders[] = {
    {.name = "child-parent-gui", .order = {TEST_RELEASE_TARGET_CHILD, TEST_RELEASE_TARGET_PARENT, TEST_RELEASE_TARGET_GUI}}, {.name = "child-gui-parent", .order = {TEST_RELEASE_TARGET_CHILD, TEST_RELEASE_TARGET_GUI, TEST_RELEASE_TARGET_PARENT}}, {.name = "parent-child-gui", .order = {TEST_RELEASE_TARGET_PARENT, TEST_RELEASE_TARGET_CHILD, TEST_RELEASE_TARGET_GUI}}, {.name = "parent-gui-child", .order = {TEST_RELEASE_TARGET_PARENT, TEST_RELEASE_TARGET_GUI, TEST_RELEASE_TARGET_CHILD}}, {.name = "gui-child-parent", .order = {TEST_RELEASE_TARGET_GUI, TEST_RELEASE_TARGET_CHILD, TEST_RELEASE_TARGET_PARENT}}, {.name = "gui-parent-child", .order = {TEST_RELEASE_TARGET_GUI, TEST_RELEASE_TARGET_PARENT, TEST_RELEASE_TARGET_CHILD}},
};

static void test_reset_state(void) {
    fake_time                 = 1000;
    test_tap_code16_count     = 0;
    test_last_tap_code16      = KC_NO;
    test_delayed_action_count = 0;
    test_last_delayed_action  = KC_NO;
    memset(test_delayed_actions, 0, sizeof(test_delayed_actions));
    current_cpi                = 0;
    fake_mods                  = 0;
    fake_weak_mods             = 0;
    fake_oneshot_mods          = 0;
    fake_oneshot_locked_mods   = 0;
    fake_managed_mods          = 0;
    fake_physical_mods         = 0;
    dragscroll_enabled         = false;
    sniping_enabled            = false;
    auto_mouse_enabled         = true;
    auto_mouse_toggled         = false;
    auto_mouse_key_tracker     = 0;
    auto_mouse_layer_target    = LAYER_POINTER;
    auto_mouse_layer_off_count = 0;
    reset_dragscroll_count     = 0;
    memset(test_pressed_keycodes, 0, sizeof(test_pressed_keycodes));
    memset(test_held_actions, 0, sizeof(test_held_actions));
    noah_runtime_reset_for_test();
    layer_state = noah_layer_state_set_user(test_layer_mask(LAYER_BASE));
}

static void test_assert_runtime_trace_snapshot_equal(const noah_runtime_trace_snapshot_t *expected, const noah_runtime_trace_snapshot_t *actual) {
    CHECK(expected != NULL);
    CHECK(actual != NULL);
    CHECK(expected->count == actual->count);
    CHECK(expected->overflowed == actual->overflowed);

    for (uint16_t index = 0; index < expected->count; index++) {
        CHECK(expected->entries[index].kind == actual->entries[index].kind);
        CHECK(expected->entries[index].event == actual->entries[index].event);
        CHECK(expected->entries[index].a == actual->entries[index].a);
        CHECK(expected->entries[index].b == actual->entries[index].b);
    }
}

static void test_shadow_replay_scenario(void (*scenario)(void)) {
    runtime_event_t               events[2048];
    noah_runtime_trace_snapshot_t original_trace;
    noah_runtime_trace_snapshot_t replay_trace;
    projection_snapshot_t         original_projection;
    projection_snapshot_t         replay_projection;
    uint16_t                      event_count;

    CHECK(scenario != NULL);

    test_reset_state();
    noah_runtime_trace_reset();
    scenario();
    noah_runtime_trace_snapshot(&original_trace);
    original_projection = key_runtime_core_projection_snapshot_capture();

    CHECK(!original_trace.overflowed);
    event_count = key_runtime_core_trace_decode_input_events(&original_trace, events, ARRAY_SIZE(events));
    CHECK(event_count != 0u);

    test_reset_state();
    noah_runtime_trace_reset();

    for (uint16_t index = 0; index < event_count; index++) {
        (void)key_runtime_integration_apply_core_event(&fake_time, &events[index]);
    }

    noah_runtime_trace_snapshot(&replay_trace);
    replay_projection = key_runtime_core_projection_snapshot_capture();

    CHECK(!replay_trace.overflowed);
    CHECK(key_runtime_core_projection_snapshot_equal(&original_projection, &replay_projection));
    test_assert_runtime_trace_snapshot_equal(&original_trace, &replay_trace);
}

int uprintf(const char *fmt, ...) {
    (void)fmt;
    return 0;
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

uint32_t last_input_activity_elapsed(void) {
    return 0;
}

uint32_t last_matrix_activity_elapsed(void) {
    return 0;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

static void test_apply_layer_state(layer_state_t next_state) {
    layer_state = noah_layer_state_set_user(next_state);
}

void layer_on(uint8_t layer) {
    test_apply_layer_state(layer_state | ((layer_state_t)1u << layer));
}

void layer_off(uint8_t layer) {
    test_apply_layer_state(layer_state & (layer_state_t) ~((layer_state_t)1u << layer));
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return keymaps[layer_num][row][column];
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
    test_tap_code16_count++;
    test_last_tap_code16 = keycode;
}

void register_code16(uint16_t keycode) {
    (void)keycode;
}

void unregister_code16(uint16_t keycode) {
    (void)keycode;
}

void register_code(uint8_t keycode) {
    fake_mods |= test_modifier_mask(keycode);
}

void unregister_code(uint8_t keycode) {
    uint8_t mask = test_modifier_mask(keycode);

    fake_mods = (uint8_t)((fake_mods & (uint8_t)~mask) | fake_physical_mods | fake_managed_mods);
}

void wait_ms(uint16_t ms) {
    (void)ms;
}

bool noah_qmk_contract_try_play_via_macro(uint16_t action) {
    (void)action;
    return false;
}

bool macro_dispatch(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool noah_synthetic_record_active(void) {
    return false;
}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    (void)keycode;
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    if (IS_QK_ONE_SHOT_MOD(keycode)) {
        fake_oneshot_mods |= QK_ONE_SHOT_MOD_GET_MODS(keycode);
    }
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

void keyboard_mod_ownership_register(uint16_t keycode);
void keyboard_mod_ownership_unregister(uint16_t keycode);
void keyboard_mod_ownership_register_mods(uint8_t mods);
void keyboard_mod_ownership_unregister_mods(uint8_t mods);

bool owned_keycode_register(uint16_t keycode) {
    if (test_modifier_mask(keycode) != 0) {
        keyboard_mod_ownership_register(keycode);
        return true;
    }

    register_code((uint8_t)keycode);
    return true;
}

bool owned_keycode_unregister(uint16_t keycode) {
    if (test_modifier_mask(keycode) != 0) {
        keyboard_mod_ownership_unregister(keycode);
        return true;
    }

    unregister_code((uint8_t)keycode);
    return true;
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
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

void keyboard_mod_ownership_register_mods(uint8_t mods) {
    fake_managed_mods |= mods;
    fake_mods |= mods;
}

void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    fake_managed_mods &= (uint8_t)~mods;
    fake_mods = (uint8_t)((fake_mods & (uint8_t)~mods) | fake_physical_mods | fake_managed_mods);
}

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    return (uint8_t)(mods & fake_managed_mods & (uint8_t)~fake_physical_mods);
}

void keyboard_mod_ownership_debug_snapshot(keyboard_mod_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (keyboard_mod_ownership_debug_snapshot_t){
        .live_state =
            {
                .real           = fake_mods,
                .weak           = fake_weak_mods,
                .oneshot        = fake_oneshot_mods,
                .oneshot_locked = fake_oneshot_locked_mods,
            },
    };

    for (uint8_t index = 0; index < KEYBOARD_MOD_OWNERSHIP_MOD_COUNT; index++) {
        uint8_t bit = (uint8_t)(1u << index);

        out->physical_refcounts[index] = (fake_physical_mods & bit) != 0 ? 1u : 0u;
        out->managed_refcounts[index]  = (fake_managed_mods & bit) != 0 ? 1u : 0u;
    }
}

void keyboard_mod_state_apply(keyboard_mod_state_t state) {
    fake_mods                = state.real;
    fake_weak_mods           = state.weak;
    fake_oneshot_mods        = state.oneshot;
    fake_oneshot_locked_mods = state.oneshot_locked;
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

bool charybdis_get_pointer_dragscroll_enabled(void) {
    return dragscroll_enabled;
}

bool charybdis_get_pointer_sniping_enabled(void) {
    return sniping_enabled;
}

uint16_t charybdis_get_pointer_default_dpi(void) {
    return 800;
}

uint16_t charybdis_get_pointer_sniping_dpi(void) {
    return 350;
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

void reset_volume_mode(void) {}

void reset_dragscroll_mode(void) {
    reset_dragscroll_count++;
}

void reset_brightness_mode(void) {}
void reset_zoom_mode(void) {}
void reset_arrow_mode(void) {}

bool is_keyboard_master(void) {
    return true;
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

void set_auto_mouse_enable(bool enable) {
    auto_mouse_enabled = enable;
}

void set_auto_mouse_layer(uint8_t layer) {
    auto_mouse_layer_target = layer;
}

void auto_mouse_toggle(void) {
    auto_mouse_toggled = !auto_mouse_toggled;
}

uint16_t auto_mouse_get_time_elapsed(void) {
    return 0;
}

bool is_auto_mouse_active(void) {
    return auto_mouse_toggled || auto_mouse_key_tracker != 0;
}

void auto_mouse_keyevent(bool pressed) {
    auto_mouse_key_tracker += pressed ? 1 : -1;
    if (auto_mouse_key_tracker < 0) {
        auto_mouse_key_tracker = 0;
    }
}

void auto_mouse_layer_off(void) {
    auto_mouse_layer_off_count++;

    if (layer_state_cmp(layer_state, auto_mouse_layer_target) && auto_mouse_enabled && !auto_mouse_toggled && auto_mouse_key_tracker == 0) {
        layer_off(auto_mouse_layer_target);
    }
}

static void test_auto_mouse_reset_trigger(bool pressed) {
    if (!pressed) {
        return;
    }

    if (layer_state_cmp(layer_state, auto_mouse_layer_target)) {
        layer_off(auto_mouse_layer_target);
    }

    auto_mouse_toggled     = false;
    auto_mouse_key_tracker = 0;
}

static bool test_is_plain_modifier_or_qk_mod(uint16_t keycode) {
    return (keycode >= KC_LEFT_CTRL && keycode <= KC_RIGHT_GUI) || (keycode >= QK_MODS && keycode <= QK_MODS_MAX);
}

bool key_runtime_integration_pre_userspace_record(uint16_t keycode, keyrecord_t *record) {
    if (!auto_mouse_enabled) {
        return true;
    }

    if (test_is_plain_modifier_or_qk_mod(keycode)) {
        return true;
    }

    if (keycode >= QK_TO && keycode <= QK_TO_MAX) {
        if (QK_TO_GET_LAYER(keycode) == auto_mouse_layer_target && !record->event.pressed) {
            auto_mouse_toggled ^= 1;
        }
        return true;
    }

    if (keycode >= QK_TOGGLE_LAYER && keycode <= QK_TOGGLE_LAYER_MAX) {
        if (QK_TOGGLE_LAYER_GET_LAYER(keycode) == auto_mouse_layer_target && !record->event.pressed) {
            auto_mouse_toggled ^= 1;
        }
        return true;
    }

    if (keycode >= QK_MOMENTARY && keycode <= QK_MOMENTARY_MAX) {
        if (QK_MOMENTARY_GET_LAYER(keycode) == auto_mouse_layer_target) {
            auto_mouse_keyevent(record->event.pressed);
        }
        return true;
    }

    if ((keycode >= QK_DEF_LAYER && keycode <= QK_DEF_LAYER_MAX) || (keycode >= QK_ONE_SHOT_LAYER && keycode <= QK_ONE_SHOT_LAYER_MAX) || (keycode >= QK_ONE_SHOT_MOD && keycode <= QK_ONE_SHOT_MOD_MAX)) {
        return true;
    }

    if (keycode >= QK_LAYER_MOD && keycode <= QK_LAYER_MOD_MAX) {
        if (QK_LAYER_MOD_GET_LAYER(keycode) == auto_mouse_layer_target) {
            auto_mouse_keyevent(record->event.pressed);
        }
        return true;
    }

    if (keycode >= QK_LAYER_TAP_TOGGLE && keycode <= QK_LAYER_TAP_TOGGLE_MAX) {
        if (QK_LAYER_TAP_TOGGLE_GET_LAYER(keycode) == auto_mouse_layer_target) {
            auto_mouse_keyevent(record->event.pressed);
        }
        return true;
    }

    if (keycode >= QK_LAYER_TAP && keycode <= QK_LAYER_TAP_MAX) {
        if (!record->tap.count) {
            if (QK_LAYER_TAP_GET_LAYER(keycode) == auto_mouse_layer_target) {
                auto_mouse_keyevent(record->event.pressed);
            }
            return true;
        }
    }

    if (keycode >= QK_MOD_TAP && keycode <= QK_MOD_TAP_MAX && !record->tap.count) {
        return true;
    }

    if (!IS_NOEVENT(record->event)) {
        if (IS_MOUSEKEY_BUTTON(keycode) || noah_is_mouse_record_user(keycode, record)) {
            auto_mouse_keyevent(record->event.pressed);
        } else if (!is_auto_mouse_active()) {
            test_auto_mouse_reset_trigger(record->event.pressed);
        }
    }

    return true;
}

bool transaction_rpc_send(int8_t transaction_id, uint8_t initiator2target_buffer_size, const void *initiator2target_buffer) {
    (void)transaction_id;
    (void)initiator2target_buffer_size;
    (void)initiator2target_buffer;
    return true;
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    (void)transaction_id;
    (void)callback;
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    test_delayed_action_count++;
    test_last_delayed_action = action;
    if (test_delayed_action_count <= ARRAY_SIZE(test_delayed_actions)) {
        test_delayed_actions[test_delayed_action_count - 1u] = action;
    }
    (void)mods;
}

void dispatch_delayed_action_at(keypos_t key_pos, uint16_t action, delayed_action_mods_t mods) {
    dispatch_delayed_action(action, mods);

    if (!IS_QK_ONE_SHOT_MOD(action)) {
        return;
    }

    keyboard_mod_state_t saved = keyboard_mod_state_suspend();

    keyboard_mod_state_apply(mods);
    noah_emit_action_tap_at(key_pos, action, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);

    keyboard_mod_state_t emitted = {
        .real           = fake_mods,
        .weak           = fake_weak_mods,
        .oneshot        = fake_oneshot_mods,
        .oneshot_locked = fake_oneshot_locked_mods,
    };

    saved.oneshot |= emitted.oneshot;
    saved.oneshot_locked |= emitted.oneshot_locked;
    keyboard_mod_state_apply(saved);
}

void held_action_register(keypos_t key_pos, uint16_t action) {
    int16_t slot = test_find_held_action_slot(key_pos);

    if (slot >= 0) {
        if (test_held_actions[slot].action == action) {
            return;
        }

        noah_action_release(key_pos, test_held_actions[slot].action);
    } else {
        slot = test_find_free_held_action_slot();
        CHECK(slot >= 0);
    }

    test_held_actions[slot] = (test_held_action_binding_t){
        .active  = true,
        .key_pos = key_pos,
        .action  = action,
    };
    noah_action_press(key_pos, action);
}

void held_action_unregister(keypos_t key_pos, uint16_t action) {
    int16_t slot = test_find_held_action_slot(key_pos);

    if (slot >= 0) {
        action                         = test_held_actions[slot].action;
        test_held_actions[slot].active = false;
        test_held_actions[slot].action = KC_NO;
    }

    noah_action_release(key_pos, action);
}

bool held_action_release_owned_by_key(keypos_t key_pos) {
    int16_t slot = test_find_held_action_slot(key_pos);

    if (slot < 0) {
        return false;
    }

    uint16_t action                = test_held_actions[slot].action;
    test_held_actions[slot].active = false;
    test_held_actions[slot].action = KC_NO;
    noah_action_release(key_pos, action);
    return true;
}

bool held_modifier_release_owned_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

void held_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    (void)key_pos;
    (void)action;
    (void)repeat_hz;
}

bool held_repeat_release_owned_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

void held_repeat_tick(void) {}

bool held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    int16_t slot = test_find_held_action_slot(key_pos);

    return slot >= 0 && test_held_actions[slot].action == action;
}

uint8_t key_feedback_pack(void) {
    return 0;
}

void key_feedback_bitmap(uint8_t *out_bitmap) {
    key_origin_bitmap_clear(out_bitmap);
}

void split_runtime_sync(void) {}

void split_runtime_sync_request(void) {}

static void test_left_thumb_double_tap_hold_toggles_num_layer(void) {
    keypos_t key_pos      = test_left_thumb_pos();
    uint16_t base_keycode = test_keycode_at(LAYER_BASE, key_pos);

    test_reset_state();

    CHECK(test_resolve_keycode(key_pos) == base_keycode);
    test_run_double_tap_hold_cycle(key_pos, 401);
    CHECK(test_layer_locked(LAYER_NUM));
    CHECK(test_layer_active(LAYER_NUM));
    CHECK(test_resolve_keycode(key_pos) == base_keycode);

    key_runtime_integration_advance(&fake_time, 40);
    test_run_double_tap_hold_cycle(key_pos, 401);
    CHECK(!test_layer_locked(LAYER_NUM));
    CHECK(!test_layer_active(LAYER_NUM));
}

static void test_right_thumb_double_tap_hold_toggles_num_layer(void) {
    keypos_t key_pos      = test_right_thumb_pos();
    uint16_t base_keycode = test_keycode_at(LAYER_BASE, key_pos);

    test_reset_state();

    CHECK(test_resolve_keycode(key_pos) == base_keycode);
    test_run_double_tap_hold_cycle(key_pos, 401);
    CHECK(test_layer_locked(LAYER_NUM));
    CHECK(test_layer_active(LAYER_NUM));
    CHECK(test_resolve_keycode(key_pos) == base_keycode);

    key_runtime_integration_advance(&fake_time, 40);
    test_run_double_tap_hold_cycle(key_pos, 401);
    CHECK(!test_layer_locked(LAYER_NUM));
    CHECK(!test_layer_active(LAYER_NUM));
}

static void test_thumb_double_tap_hold_with_intermediate_scan_toggles_num_layer_once_per_cycle(void) {
    keypos_t key_pos = test_right_thumb_pos();

    test_reset_state();

    test_run_double_tap_hold_cycle_with_intermediate_scan(key_pos, 120, 281);
    CHECK(test_layer_locked(LAYER_NUM));
    CHECK(test_layer_active(LAYER_NUM));

    key_runtime_integration_advance(&fake_time, 40);
    test_run_double_tap_hold_cycle_with_intermediate_scan(key_pos, 120, 281);
    CHECK(!test_layer_locked(LAYER_NUM));
    CHECK(!test_layer_active(LAYER_NUM));
}

static void test_left_thumb_double_tap_hold_escape_feedback_sequence(void) {
    keypos_t key_pos      = test_left_thumb_pos();
    uint16_t base_keycode = test_keycode_at(LAYER_BASE, key_pos);

    test_reset_state();

    CHECK(test_resolve_keycode(key_pos) == base_keycode);
    test_press_resolved(key_pos);
    test_release_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, 40);
    test_press_resolved(key_pos);

    CHECK(noah_runtime_debug_slot_pending_multi_tap_count(key_pos) == 2);
    CHECK(noah_runtime_debug_slot_pending_multi_tap_holding(key_pos));
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 0u);

    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();

    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 2u);

    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_BRANCH_CONFIRM_TERM + 1);
    key_runtime_integration_scan();

    CHECK(noah_runtime_debug_slot_phase(key_pos) == KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_HOLD_PENDING);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 0u);

    test_release_resolved(key_pos);

    CHECK(test_delayed_action_count == 1u);
    CHECK(test_last_delayed_action == KC_ESC);
    test_assert_thumb_runtime_quiescent(key_pos);
}

static void test_left_thumb_double_tap_hold_escape_release_crossing_threshold_pulses_branch(void) {
    keypos_t key_pos      = test_left_thumb_pos();
    uint16_t base_keycode = test_keycode_at(LAYER_BASE, key_pos);

    test_reset_state();

    CHECK(test_resolve_keycode(key_pos) == base_keycode);
    test_press_resolved(key_pos);
    test_release_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, 40);
    test_press_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
    test_release_resolved(key_pos);

    CHECK(test_delayed_action_count == 0u);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 2u);

    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_BRANCH_CONFIRM_TERM + 1);
    key_runtime_integration_scan();

    CHECK(test_delayed_action_count == 1u);
    CHECK(test_last_delayed_action == KC_ESC);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_HOLD_PENDING);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 0u);

    key_runtime_integration_advance(&fake_time, RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS + 1);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    test_assert_thumb_runtime_quiescent(key_pos);
}

static void test_left_thumb_double_tap_hold_escape_release_during_branch_keeps_hold_feedback_pulse(void) {
    keypos_t key_pos      = test_left_thumb_pos();
    uint16_t base_keycode = test_keycode_at(LAYER_BASE, key_pos);

    test_reset_state();

    CHECK(test_resolve_keycode(key_pos) == base_keycode);
    test_press_resolved(key_pos);
    test_release_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, 40);
    test_press_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();

    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 2u);

    test_release_resolved(key_pos);

    CHECK(test_delayed_action_count == 0u);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 2u);

    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_BRANCH_CONFIRM_TERM + 1);
    key_runtime_integration_scan();

    CHECK(test_delayed_action_count == 1u);
    CHECK(test_last_delayed_action == KC_ESC);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_HOLD_PENDING);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 0u);

    key_runtime_integration_advance(&fake_time, RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS + 1);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    test_assert_thumb_runtime_quiescent(key_pos);
}

static void test_left_thumb_double_tap_long_hold_num_feedback_replaces_branch(void) {
    keypos_t key_pos      = test_left_thumb_pos();
    uint16_t base_keycode = test_keycode_at(LAYER_BASE, key_pos);
    uint16_t branch_scan_elapsed = (uint16_t)(CUSTOM_TAP_HOLD_TERM + 100u);

    test_reset_state();

    CHECK(test_resolve_keycode(key_pos) == base_keycode);
    test_press_resolved(key_pos);
    test_release_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, 40);
    test_press_resolved(key_pos);

    key_runtime_integration_advance(&fake_time, branch_scan_elapsed);
    key_runtime_integration_scan();
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 2u);

    key_runtime_integration_advance(&fake_time, (uint16_t)(CUSTOM_LONGER_HOLD_TERM - branch_scan_elapsed + 1u));
    key_runtime_integration_scan();

    CHECK(test_layer_locked(LAYER_NUM));
    CHECK(test_layer_active(LAYER_NUM));
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 0u);

    test_release_resolved(key_pos);
    CHECK(test_layer_locked(LAYER_NUM));
}

static void test_left_and_right_thumb_single_taps_keep_independent_pending_chains(void) {
    keypos_t left_thumb_pos  = test_left_thumb_pos();
    keypos_t right_thumb_pos = test_right_thumb_pos();

    test_reset_state();

    test_run_quick_tap(left_thumb_pos);
    test_advance_thumb_multi_tap_gap();
    test_run_quick_tap(right_thumb_pos);

    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 2);
    CHECK(noah_runtime_debug_slot_has_pending_multi_tap(left_thumb_pos));
    CHECK(noah_runtime_debug_slot_has_pending_multi_tap(right_thumb_pos));
    CHECK(test_delayed_action_count == 0);
    CHECK(test_feedback_semantic_for_key(left_thumb_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    CHECK(test_feedback_semantic_for_key(right_thumb_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    CHECK(test_feedback_tap_branch_for_key(left_thumb_pos) == 0u);
    CHECK(test_feedback_tap_branch_for_key(right_thumb_pos) == 0u);

    key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM + 1);
    key_runtime_integration_scan();

    CHECK(test_delayed_action_count == 2);
    CHECK(test_delayed_actions[0] == LOCK_LAYER(LAYER_SYM));
    CHECK(test_delayed_actions[1] == LOCK_LAYER(LAYER_NAV));
    CHECK(!noah_runtime_debug_slot_has_pending_multi_tap(left_thumb_pos));
    CHECK(!noah_runtime_debug_slot_has_pending_multi_tap(right_thumb_pos));
    test_assert_no_tap_branch_feedback(left_thumb_pos);
    test_assert_no_tap_branch_feedback(right_thumb_pos);
    test_assert_thumb_runtime_quiescent(left_thumb_pos);
    test_assert_thumb_runtime_quiescent(right_thumb_pos);
}

static void test_number_key_hold_only_tap_feedback_stays_quiet(void) {
    keypos_t key_pos = test_find_keypos_on_layer(LAYER_BASE, KC_6);

    CHECK(test_keypos_valid(key_pos));
    CHECK(test_resolve_keycode(key_pos) == KC_6);

    test_reset_state();

    test_run_quick_tap(key_pos);

    CHECK(noah_runtime_debug_slot_pending_multi_tap_count(key_pos) == 0u);
    CHECK(test_tap_code16_count == 1u);
    CHECK(test_last_tap_code16 == KC_6);
    CHECK(test_delayed_action_count == 0u);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 0u);

    key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM + 1u);
    key_runtime_integration_scan();

    CHECK(test_tap_code16_count == 1u);
    CHECK(test_delayed_action_count == 0u);
    CHECK(!noah_runtime_debug_slot_has_pending_multi_tap(key_pos));
    test_assert_no_tap_branch_feedback(key_pos);
}

static void test_number_key_hold_only_repeated_taps_do_not_enter_branch_feedback(void) {
    keypos_t key_pos = test_find_keypos_on_layer(LAYER_BASE, KC_6);

    CHECK(test_keypos_valid(key_pos));
    CHECK(test_resolve_keycode(key_pos) == KC_6);

    test_reset_state();

    test_run_quick_tap(key_pos);
    key_runtime_integration_advance(&fake_time, 40u);
    test_run_quick_tap(key_pos);

    CHECK(noah_runtime_debug_slot_pending_multi_tap_count(key_pos) == 0u);
    CHECK(test_tap_code16_count == 2u);
    CHECK(test_last_tap_code16 == KC_6);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 0u);

    key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM + 1u);
    key_runtime_integration_scan();

    CHECK(test_tap_code16_count == 2u);
    CHECK(test_delayed_action_count == 0u);
    test_assert_no_tap_branch_feedback(key_pos);
}

static void test_right_thumb_triple_tap_flushes_next_track_after_timeout(void) {
    keypos_t key_pos      = test_right_thumb_pos();
    uint16_t base_keycode = test_keycode_at(LAYER_BASE, key_pos);

    test_reset_state();

    CHECK(test_resolve_keycode(key_pos) == base_keycode);
    test_run_quick_tap(key_pos);
    test_advance_thumb_multi_tap_gap();
    test_run_quick_tap(key_pos);
    test_advance_thumb_multi_tap_gap();
    test_run_quick_tap(key_pos);

    CHECK(noah_runtime_debug_slot_pending_multi_tap_count(key_pos) == 3);
    CHECK(!noah_runtime_debug_slot_pending_multi_tap_holding(key_pos));
    CHECK(test_tap_code16_count == 0);

    key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM + 1);
    key_runtime_integration_scan();

    CHECK(test_tap_code16_count == 0);
    CHECK(test_delayed_action_count == 0);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);

    test_finish_tap_branch_confirmation();

    CHECK(test_delayed_action_count == 1);
    CHECK(test_last_delayed_action == KC_MNXT);
    test_assert_thumb_runtime_quiescent(key_pos);
}

static void test_right_thumb_triple_tap_long_hold_registers_next_track_hold(void) {
    keypos_t key_pos      = test_right_thumb_pos();
    uint16_t base_keycode = test_keycode_at(LAYER_BASE, key_pos);

    test_reset_state();

    CHECK(test_resolve_keycode(key_pos) == base_keycode);
    test_run_quick_tap(key_pos);
    test_advance_thumb_multi_tap_gap();
    test_run_quick_tap(key_pos);
    test_advance_thumb_multi_tap_gap();
    test_press_resolved(key_pos);

    CHECK(noah_runtime_debug_slot_pending_multi_tap_count(key_pos) == 3);
    CHECK(noah_runtime_debug_slot_pending_multi_tap_holding(key_pos));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == base_keycode);

    key_runtime_integration_advance(&fake_time, CUSTOM_LONGER_HOLD_TERM + 1);
    key_runtime_integration_scan();

    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
    CHECK(test_feedback_semantic_for_key(key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
    CHECK(test_feedback_tap_branch_for_key(key_pos) == 3u);

    test_finish_tap_branch_confirmation();

    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_MNXT);
    CHECK(test_tap_code16_count == 0);

    test_release_resolved(key_pos);
    test_assert_thumb_runtime_quiescent(key_pos);
}

static void test_right_thumb_quadruple_tap_dispatches_previous_track(void) {
    keypos_t key_pos      = test_right_thumb_pos();
    uint16_t base_keycode = test_keycode_at(LAYER_BASE, key_pos);

    test_reset_state();

    CHECK(test_resolve_keycode(key_pos) == base_keycode);
    test_run_quick_tap(key_pos);
    test_advance_thumb_multi_tap_gap();
    test_run_quick_tap(key_pos);
    test_advance_thumb_multi_tap_gap();
    test_run_quick_tap(key_pos);
    test_advance_thumb_multi_tap_gap();
    test_run_quick_tap(key_pos);

    CHECK(test_tap_code16_count == 0);
    CHECK(test_delayed_action_count == 0);

    key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM + 1);
    key_runtime_integration_scan();

    test_finish_tap_branch_confirmation();

    CHECK(test_delayed_action_count == 1);
    CHECK(test_last_delayed_action == KC_MPRV);
    test_assert_thumb_runtime_quiescent(key_pos);
}

static void test_pointer_pinch_double_tap_queues_zoom_chord(void) {
    keypos_t pinch_pos = test_find_keypos_on_layer(LAYER_POINTER, PINCH_MODE);

    CHECK(test_keypos_valid(pinch_pos));

    test_reset_state();
    layer_state = noah_layer_state_set_user(test_layer_mask(LAYER_BASE) | test_layer_mask(LAYER_POINTER));

    CHECK(test_resolve_keycode(pinch_pos) == PINCH_MODE);
    test_run_quick_tap(pinch_pos);
    test_advance_thumb_multi_tap_gap();
    test_run_quick_tap(pinch_pos);

    if (test_delayed_action_count == 0) {
        key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM + 1);
        key_runtime_integration_scan();
    }
    if (test_delayed_action_count == 0) {
        test_finish_tap_branch_confirmation();
    }

    CHECK(test_tap_code16_count == 0);
    CHECK(test_delayed_action_count == 1);
    CHECK(test_last_delayed_action == VIA_MACRO_6);
    CHECK(pd_mode_local_active_snapshot() == 0);
    test_assert_thumb_runtime_quiescent(pinch_pos);
}

static void test_pointer_pinch_double_tap_salvos_queue_zoom_chord_cleanly(void) {
    keypos_t pinch_pos = test_find_keypos_on_layer(LAYER_POINTER, PINCH_MODE);

    CHECK(test_keypos_valid(pinch_pos));

    test_reset_state();
    layer_state = noah_layer_state_set_user(test_layer_mask(LAYER_BASE) | test_layer_mask(LAYER_POINTER));

    for (uint8_t salvo = 0; salvo < 4; salvo++) {
        test_run_quick_tap(pinch_pos);
        test_advance_thumb_multi_tap_gap();
        test_run_quick_tap(pinch_pos);

        if (test_delayed_action_count == salvo) {
            key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM + 1);
            key_runtime_integration_scan();
        }
        if (test_delayed_action_count == salvo) {
            test_finish_tap_branch_confirmation();
        }

        CHECK(test_tap_code16_count == 0);
        CHECK(test_delayed_action_count == (uint16_t)(salvo + 1u));
        CHECK(test_last_delayed_action == VIA_MACRO_6);
        CHECK(pd_mode_local_active_snapshot() == 0);
        test_assert_thumb_runtime_quiescent(pinch_pos);

        key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM + 1);
        key_runtime_integration_scan();
    }
}

static void test_pointer_terminal_double_tap_action_defers_on_exact_third_press(uint16_t keycode, uint16_t expected_action) {
    keypos_t key_pos;
    keypos_t deferred_key_pos;

    key_pos = test_find_keypos_on_layer(LAYER_POINTER, keycode);
    CHECK(test_keypos_valid(key_pos));

    test_reset_state();
    layer_state = noah_layer_state_set_user(test_layer_mask(LAYER_BASE) | test_layer_mask(LAYER_POINTER));

    CHECK(test_resolve_keycode(key_pos) == keycode);
    test_run_quick_tap(key_pos);
    test_advance_thumb_multi_tap_gap();
    test_run_quick_tap(key_pos);
    test_advance_thumb_multi_tap_gap();

    test_press_resolved(key_pos);

    CHECK(test_tap_code16_count == 0);
    CHECK(test_delayed_action_count == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 1u);
    CHECK(noah_runtime_debug_deferred_release_action(0u) == expected_action);
    CHECK(noah_runtime_debug_deferred_release_key_pos(0u, &deferred_key_pos));
    CHECK(test_keypos_equal(deferred_key_pos, key_pos));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == keycode);

    key_runtime_integration_scan();

    CHECK(test_delayed_action_count == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 1u);

    test_release_resolved(key_pos);

    CHECK(test_delayed_action_count == 1);
    CHECK(test_last_delayed_action == expected_action);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(key_pos) == KC_NO);
}

static void test_pointer_pinch_exact_third_tap_defers_zoom_chord_until_release(void) {
    test_pointer_terminal_double_tap_action_defers_on_exact_third_press(PINCH_MODE, VIA_MACRO_6);
}

static void test_pointer_volume_exact_third_tap_defers_mute_until_release(void) {
    test_pointer_terminal_double_tap_action_defers_on_exact_third_press(VOLUME_MODE, KC_MUTE);
}

static void test_click_spam_combo_uses_authored_combo_owner(void) {
    static const uint16_t click_spam_combo_keys[] = {
        MS_BTN1,
        MS_BTN2,
    };

    uint8_t  bitmap[KEY_ORIGIN_BITMAP_SIZE];
    keypos_t active_slot_key_pos;
    keypos_t btn1_pos;
    keypos_t btn2_pos;
    int16_t  combo_index;
    uint16_t click_spam_keycode;

    test_reset_state();
    layer_state = noah_layer_state_set_user(test_layer_mask(LAYER_BASE) | test_layer_mask(LAYER_POINTER));

    btn1_pos = test_find_keypos_on_layer(LAYER_POINTER, MS_BTN1);
    btn2_pos = test_find_keypos_on_layer(LAYER_POINTER, MS_BTN2);

    CHECK(test_keypos_valid(btn1_pos));
    CHECK(test_keypos_valid(btn2_pos));
    CHECK(test_resolve_keycode(btn1_pos) == MS_BTN1);
    CHECK(test_resolve_keycode(btn2_pos) == MS_BTN2);

    combo_index = test_find_combo_index_for_exact_keys(click_spam_combo_keys, ARRAY_SIZE(click_spam_combo_keys));
    CHECK(combo_index >= 0);
    click_spam_keycode = key_combos[combo_index].keycode;

    test_observe_combo_member(btn1_pos, true);
    test_observe_combo_member(btn2_pos, true);
    key_combos[combo_index].active = true;

    CHECK(!test_process_combo_output(click_spam_keycode, true));
    CHECK(noah_runtime_debug_active_slot_count() == 1u);
    CHECK(noah_runtime_debug_active_slot_key_pos(0u, &active_slot_key_pos));
    CHECK(test_keypos_equal(active_slot_key_pos, btn2_pos));
    CHECK(noah_runtime_debug_slot_owner_keycode(btn2_pos) == click_spam_keycode);
    CHECK(key_origin_registry_get_bitmap(btn2_pos, bitmap));
    CHECK(key_origin_bitmap_has_keypos(bitmap, btn1_pos));
    CHECK(key_origin_bitmap_has_keypos(bitmap, btn2_pos));

    key_runtime_integration_advance(&fake_time, 2u);
    key_runtime_integration_scan();

    CHECK(noah_runtime_debug_slot_owner_keycode(btn2_pos) == click_spam_keycode);
    CHECK(test_tap_code16_count == 0u);

    key_combos[combo_index].active = false;
    test_observe_combo_member(btn2_pos, false);
    test_observe_combo_member(btn1_pos, false);

    CHECK(!test_process_combo_output(click_spam_keycode, false));
    key_runtime_integration_scan();

    CHECK(noah_runtime_debug_active_slot_count() == 0u);
    CHECK(noah_runtime_debug_slot_owner_keycode(btn2_pos) == KC_NO);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0u);
}

static void test_cmd_combo_double_tap_hold_uses_stable_owner_across_press_order(void) {
    static const uint16_t cmd_combo_keys[] = {
        KC_N,
        KC_M,
    };

    keypos_t n_pos;
    keypos_t m_pos;
    int16_t  combo_index;
    uint16_t combo_keycode;

    test_reset_state();

    n_pos = test_find_keypos_on_layer(LAYER_BASE, KC_N);
    m_pos = test_find_keypos_on_layer(LAYER_BASE, KC_M);

    CHECK(test_keypos_valid(n_pos));
    CHECK(test_keypos_valid(m_pos));
    CHECK(test_resolve_keycode(n_pos) == KC_N);
    CHECK(test_resolve_keycode(m_pos) == KC_M);

    combo_index = test_find_combo_index_for_exact_keys(cmd_combo_keys, ARRAY_SIZE(cmd_combo_keys));
    CHECK(combo_index >= 0);
    combo_keycode = key_combos[combo_index].keycode;
    CHECK(combo_keycode == KC_LEFT_GUI);

    test_observe_combo_member(n_pos, true);
    test_observe_combo_member(m_pos, true);
    key_combos[combo_index].active = true;
    CHECK(!test_process_combo_output(combo_keycode, true));
    CHECK(noah_runtime_debug_slot_owner_keycode(m_pos) == KC_LEFT_GUI);
    CHECK((fake_mods & MOD_BIT(KC_LEFT_GUI)) == 0u);

    key_combos[combo_index].active = false;
    test_observe_combo_member(m_pos, false);
    test_observe_combo_member(n_pos, false);
    CHECK(!test_process_combo_output(combo_keycode, false));
    CHECK(noah_runtime_debug_slot_pending_multi_tap_count(m_pos) == 1u);
    CHECK(test_feedback_semantic_for_key(m_pos) == KEY_FEEDBACK_SEMANTIC_NONE);

    key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM - 10u);

    test_observe_combo_member(m_pos, true);
    test_observe_combo_member(n_pos, true);

    CHECK(test_feedback_semantic_for_key(m_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    CHECK(test_feedback_semantic_for_key(n_pos) == KEY_FEEDBACK_SEMANTIC_NONE);

    key_runtime_integration_advance(&fake_time, COMBO_TERM + 1u);
    test_observe_combo_member(n_pos, false);

    key_combos[combo_index].active = true;
    CHECK(!test_process_combo_output(combo_keycode, true));
    CHECK(noah_runtime_debug_slot_owner_keycode(m_pos) == KC_LEFT_GUI);
    CHECK(noah_runtime_debug_slot_pending_multi_tap_holding(m_pos));
    CHECK((fake_mods & (MOD_BIT(KC_LEFT_GUI) | MOD_BIT(KC_LEFT_ALT))) == 0u);

    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1u);
    key_runtime_integration_scan();

    CHECK(noah_runtime_debug_slot_owner_keycode(m_pos) == KC_LEFT_GUI);
    CHECK(!noah_runtime_debug_slot_pending_multi_tap_holding(m_pos));
    CHECK(test_feedback_semantic_for_key(m_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);

    test_finish_tap_branch_confirmation();

    CHECK(noah_runtime_debug_slot_held_action_keycode(m_pos) == KC_LEFT_ALT);
    CHECK((fake_mods & MOD_BIT(KC_LEFT_ALT)) != 0u);
    CHECK((fake_mods & MOD_BIT(KC_LEFT_GUI)) == 0u);

    key_combos[combo_index].active = false;
    test_observe_combo_member(m_pos, false);
    CHECK(!test_process_combo_output(combo_keycode, false));
    key_runtime_integration_scan();

    CHECK(noah_runtime_debug_slot_owner_keycode(m_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(m_pos) == KC_NO);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0u);
    CHECK(fake_mods == 0u);
    CHECK(fake_managed_mods == 0u);
    CHECK(fake_physical_mods == 0u);
}

static void test_right_nav_layer_hold_dispatches_nav_taps_immediately(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);
    keypos_t       nav_left_pos     = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);

    test_reset_state();

    CHECK(test_keypos_valid(nav_hold_pos));
    CHECK(test_keypos_valid(nav_left_pos));
    CHECK(test_resolve_keycode(nav_hold_pos) == nav_hold_keycode);

    test_press_resolved(nav_hold_pos);
    CHECK(test_layer_active(LAYER_NAV));
    CHECK(pd_mode_auto_sniping_layer_active());
    CHECK(!sniping_enabled);
    CHECK(current_cpi == 0);

    key_runtime_integration_scan();
    CHECK(test_layer_active(LAYER_NAV));
    CHECK(pd_mode_auto_sniping_layer_active());
    CHECK(!sniping_enabled);
    CHECK(current_cpi == charybdis_get_pointer_sniping_dpi());

    key_runtime_integration_advance(&fake_time, 20);
    CHECK(test_resolve_keycode(nav_left_pos) == KC_LEFT);

    test_press_resolved(nav_left_pos);
    test_release_resolved(nav_left_pos);

    CHECK(test_tap_code16_count == 1);
    CHECK(test_last_tap_code16 == KC_LEFT);
    CHECK(test_delayed_action_count == 0);
    CHECK(test_layer_active(LAYER_NAV));

    test_release_resolved(nav_hold_pos);
    key_runtime_integration_scan();

    CHECK(test_tap_code16_count == 1);
    CHECK(test_delayed_action_count == 0);
    CHECK(test_last_delayed_action == KC_NO);
    CHECK(!pd_mode_auto_sniping_layer_active());
    CHECK(!sniping_enabled);
    CHECK(current_cpi == charybdis_get_pointer_default_dpi());
}

static void test_right_thumb_hold_dispatches_nav_taps_immediately(void) {
    keypos_t right_thumb_pos     = test_right_thumb_pos();
    keypos_t nav_left_pos        = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);
    uint16_t right_thumb_keycode = test_keycode_at(LAYER_BASE, right_thumb_pos);

    test_reset_state();

    CHECK(test_keypos_valid(nav_left_pos));
    CHECK(right_thumb_keycode != KC_TRNS);
    CHECK(test_resolve_keycode(right_thumb_pos) == right_thumb_keycode);

    test_press_resolved(right_thumb_pos);
    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();

    CHECK(test_layer_active(LAYER_NAV));
    CHECK(noah_runtime_debug_slot_owner_keycode(right_thumb_pos) == right_thumb_keycode);

    test_press_resolved(nav_left_pos);
    test_release_resolved(nav_left_pos);

    CHECK(test_tap_code16_count == 1);
    CHECK(test_last_tap_code16 == KC_LEFT);
    CHECK(test_delayed_action_count == 0);
    CHECK(test_layer_active(LAYER_NAV));

    test_release_resolved(right_thumb_pos);
    key_runtime_integration_scan();

    CHECK(test_tap_code16_count == 1);
    CHECK(test_delayed_action_count == 0);
    CHECK(test_last_delayed_action == KC_NO);
    CHECK(!test_layer_active(LAYER_NAV));
}

static void test_assert_dragscroll_overlap_quiescent(keypos_t parent_pos, keypos_t child_pos) {
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(parent_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(parent_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == KC_NO);
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));
    CHECK(!test_layer_active(LAYER_NAV));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(reset_dragscroll_count == 1);
    CHECK(current_cpi == charybdis_get_pointer_default_dpi());
    CHECK(fake_mods == 0);
    CHECK(fake_weak_mods == 0);
    CHECK(fake_oneshot_mods == 0);
    CHECK(fake_oneshot_locked_mods == 0);
    CHECK(fake_managed_mods == 0);
    CHECK(fake_physical_mods == 0);
    CHECK(!dragscroll_enabled);
    CHECK(!sniping_enabled);
    CHECK(auto_mouse_key_tracker == 0);
}

static void test_assert_direct_dragscroll_quiescent(keypos_t dragscroll_pos, uint8_t expected_reset_count) {
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(dragscroll_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(dragscroll_pos) == KC_NO);
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(reset_dragscroll_count == expected_reset_count);
    CHECK(fake_mods == 0);
    CHECK(fake_weak_mods == 0);
    CHECK(fake_oneshot_mods == 0);
    CHECK(fake_oneshot_locked_mods == 0);
    CHECK(fake_managed_mods == 0);
    CHECK(fake_physical_mods == 0);
    CHECK(!dragscroll_enabled);
    CHECK(auto_mouse_key_tracker == 0);
}

static void test_direct_dragscroll_repeated_quick_taps_stay_quiescent(uint8_t layer_num) {
    keypos_t dragscroll_pos = test_find_keypos_on_layer(layer_num, DRAGSCROLL);
    uint8_t  expected_reset_count = 0u;

    CHECK(test_keypos_valid(dragscroll_pos));

    test_reset_state();
    layer_state = noah_layer_state_set_user(test_layer_mask(LAYER_BASE) | test_layer_mask(layer_num));

    for (uint8_t iteration = 0; iteration < 8u; iteration++) {
        CHECK(test_resolve_keycode(dragscroll_pos) == DRAGSCROLL);

        test_press_resolved(dragscroll_pos);
        if (pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL) {
            expected_reset_count++;
            CHECK(pd_mode_local_locked_snapshot() == 0);
            CHECK(noah_runtime_debug_slot_owner_keycode(dragscroll_pos) == DRAGSCROLL);
            CHECK(noah_runtime_debug_slot_held_action_keycode(dragscroll_pos) == DRAGSCROLL);
            CHECK(auto_mouse_key_tracker == 1);
        }

        test_release_resolved(dragscroll_pos);
        key_runtime_integration_scan();
        test_assert_direct_dragscroll_quiescent(dragscroll_pos, expected_reset_count);

        if ((iteration % 2u) == 0u) {
            CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 1u);
            CHECK(noah_runtime_debug_slot_pending_multi_tap_count(dragscroll_pos) == 1u);
        } else {
            CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0u);
            CHECK(noah_runtime_debug_slot_pending_multi_tap_count(dragscroll_pos) == 0u);
        }

        key_runtime_integration_advance(&fake_time, 10u);
    }

    key_runtime_integration_advance(&fake_time, CUSTOM_MULTI_TAP_TERM + 1);
    key_runtime_integration_scan();
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
}

static void test_nav_dragscroll_repeated_quick_taps_stay_quiescent(void) {
    test_direct_dragscroll_repeated_quick_taps_stay_quiescent(LAYER_NAV);
}

static void test_pointer_dragscroll_repeated_quick_taps_stay_quiescent(void) {
    test_direct_dragscroll_repeated_quick_taps_stay_quiescent(LAYER_POINTER);
}

static void test_same_locked_nav_dragscroll_press_unlocks_and_holds_until_release(void) {
    keypos_t dragscroll_pos = test_find_keypos_on_layer(LAYER_NAV, DRAGSCROLL);
    uint16_t previous_tap_count;

    CHECK(test_keypos_valid(dragscroll_pos));

    test_reset_state();
    layer_state = noah_layer_state_set_user(test_layer_mask(LAYER_BASE) | test_layer_mask(LAYER_NAV));
    CHECK(test_resolve_keycode(dragscroll_pos) == DRAGSCROLL);

    CHECK(pd_mode_set_lock_state(PD_MODE_DRAGSCROLL, true));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_DRAGSCROLL);
    CHECK(reset_dragscroll_count == 0u);

    previous_tap_count = test_tap_code16_count;
    test_press_resolved(dragscroll_pos);

    CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(dragscroll_pos) == DRAGSCROLL);
    CHECK(noah_runtime_debug_slot_held_action_keycode(dragscroll_pos) == DRAGSCROLL);
    CHECK(test_tap_code16_count == previous_tap_count);
    CHECK(reset_dragscroll_count == 1u);

    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(test_tap_code16_count == previous_tap_count);

    test_release_resolved(dragscroll_pos);
    key_runtime_integration_scan();
    CHECK(test_tap_code16_count == previous_tap_count);
    test_assert_direct_dragscroll_quiescent(dragscroll_pos, 2u);
}

static void test_dragscroll_overlap_stays_quiescent(keypos_t parent_pos, bool requires_parent_scan) {
    keypos_t dragscroll_pos = test_find_keypos_on_layer(LAYER_NAV, DRAGSCROLL);
    keypos_t follow_on_pos  = test_find_keypos_on_layer(LAYER_BASE, KC_C);

    CHECK(test_keypos_valid(parent_pos));
    CHECK(test_keypos_valid(dragscroll_pos));
    CHECK(test_keypos_valid(follow_on_pos));

    for (uint8_t index = 0; index < 2; index++) {
        bool release_parent_first = index != 0;

        test_reset_state();

        test_press_resolved(parent_pos);
        if (requires_parent_scan) {
            key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
            key_runtime_integration_scan();
        }

        CHECK(test_layer_active(LAYER_NAV));
        CHECK(test_resolve_keycode(dragscroll_pos) == DRAGSCROLL);
        test_press_resolved(dragscroll_pos);
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
        CHECK(pd_mode_local_locked_snapshot() == 0);
        CHECK(noah_runtime_debug_slot_owner_keycode(dragscroll_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_slot_held_action_keycode(dragscroll_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
        CHECK(auto_mouse_key_tracker == 1);
        CHECK(reset_dragscroll_count == 0);

        key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
        key_runtime_integration_scan();
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
        CHECK(noah_runtime_debug_slot_owner_keycode(dragscroll_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_slot_held_action_keycode(dragscroll_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
        CHECK(auto_mouse_key_tracker == 1);

        if (release_parent_first) {
            test_release_resolved(parent_pos);
            CHECK(noah_runtime_debug_slot_owner_keycode(parent_pos) == KC_NO);
            CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
        }

        test_release_resolved(dragscroll_pos);

        if (!release_parent_first) {
            test_release_resolved(parent_pos);
        }

        key_runtime_integration_scan();
        test_assert_dragscroll_overlap_quiescent(parent_pos, dragscroll_pos);

        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, true));
        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, false));
        test_assert_dragscroll_overlap_quiescent(parent_pos, dragscroll_pos);
    }
}

static void test_raw_nav_layer_hold_enters_dragscroll_mode_cleanly(void) {
    keypos_t nav_hold_pos = test_find_keypos_on_layer(LAYER_BASE, LT(LAYER_NAV, KC_SLSH));

    test_dragscroll_overlap_stays_quiescent(nav_hold_pos, false);
}

static void test_right_thumb_nav_hold_enters_dragscroll_mode_cleanly(void) {
    test_dragscroll_overlap_stays_quiescent(test_right_thumb_pos(), true);
}

static void test_activate_gui_double_tap_alt_hold(keypos_t gui_pos) {
    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_resolve_keycode(gui_pos) == KC_LEFT_GUI);

    test_press_resolved(gui_pos);
    test_release_resolved(gui_pos);

    key_runtime_integration_advance(&fake_time, 40);
    test_press_resolved(gui_pos);

    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();

    CHECK(noah_runtime_debug_slot_owner_keycode(gui_pos) == KC_LEFT_GUI);
    CHECK(noah_runtime_debug_slot_held_action_keycode(gui_pos) == KC_NO);
    CHECK(test_feedback_semantic_for_key(gui_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);

    test_finish_tap_branch_confirmation();

    CHECK(noah_runtime_debug_slot_held_action_keycode(gui_pos) == KC_LEFT_ALT);
    CHECK((fake_mods & MOD_BIT(KC_LEFT_ALT)) != 0);
}

static void test_activate_gui_double_tap_alt_hold_pending(keypos_t gui_pos) {
    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_resolve_keycode(gui_pos) == KC_LEFT_GUI);

    test_press_resolved(gui_pos);
    test_release_resolved(gui_pos);

    key_runtime_integration_advance(&fake_time, 40);
    test_press_resolved(gui_pos);

    CHECK(noah_runtime_debug_slot_owner_keycode(gui_pos) == KC_LEFT_GUI);
    CHECK(noah_runtime_debug_slot_held_action_keycode(gui_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_pending_multi_tap_holding(gui_pos));
    CHECK(noah_runtime_debug_slot_phase(gui_pos) == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    CHECK((fake_mods & MOD_BIT(KC_LEFT_ALT)) == 0);
}

static void test_commit_pending_gui_double_tap_alt_hold(keypos_t gui_pos) {
    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();

    CHECK(noah_runtime_debug_slot_owner_keycode(gui_pos) == KC_LEFT_GUI);
    CHECK(!noah_runtime_debug_slot_pending_multi_tap_holding(gui_pos));

    if (noah_runtime_debug_slot_held_action_keycode(gui_pos) == KC_NO) {
        CHECK(test_feedback_semantic_for_key(gui_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_COMMITTED);
        test_finish_tap_branch_confirmation();
    }

    CHECK(noah_runtime_debug_slot_held_action_keycode(gui_pos) == KC_LEFT_ALT);
    CHECK((fake_mods & MOD_BIT(KC_LEFT_ALT)) != 0);
}

static void test_gui_triple_tap_osm_flushes_before_next_plain_key(void) {
    keypos_t gui_pos       = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t follow_on_pos = test_find_keypos_on_layer(LAYER_BASE, KC_C);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(follow_on_pos));

    test_reset_state();

    test_run_quick_tap(gui_pos);
    key_runtime_integration_advance(&fake_time, 40);
    test_run_quick_tap(gui_pos);
    key_runtime_integration_advance(&fake_time, 40);
    test_run_quick_tap(gui_pos);

    CHECK(noah_runtime_debug_slot_pending_multi_tap_count(gui_pos) == 3u);
    CHECK(test_delayed_action_count == 0u);

    CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, true));

    CHECK(test_delayed_action_count == 1u);
    CHECK(test_last_delayed_action == OSM(MOD_LSFT));
    CHECK(fake_oneshot_mods == MOD_LSFT);
    CHECK(!noah_runtime_debug_slot_has_pending_multi_tap(gui_pos));
    CHECK(noah_runtime_debug_slot_owner_keycode(gui_pos) == KC_NO);

    CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, false));
    CHECK(noah_runtime_debug_active_slot_count() == 0u);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0u);
}

static void test_activate_nav_parent_hold(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    CHECK(test_resolve_keycode(parent_pos) == parent_keycode);
    test_press_resolved(parent_pos);
    if (requires_threshold_scan) {
        key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
        key_runtime_integration_scan();
    }

    CHECK(test_layer_active(LAYER_NAV));
}

static void test_gui_double_tap_hold_with_right_alt_arrow_mode_lock_stays_usable(void) {
    keypos_t gui_pos       = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t right_alt_pos = test_find_keypos_on_layer(LAYER_BASE, KC_RIGHT_ALT);
    keypos_t follow_on_pos = test_find_keypos_on_layer(LAYER_BASE, KC_C);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(right_alt_pos));
    CHECK(test_keypos_valid(follow_on_pos));

    for (uint8_t index = 0; index < 2; index++) {
        bool release_gui_first = index != 0;

        test_reset_state();
        test_activate_gui_double_tap_alt_hold(gui_pos);

        test_press_resolved(right_alt_pos);
        CHECK(noah_runtime_debug_slot_owner_keycode(right_alt_pos) == KC_RIGHT_ALT);
        CHECK(noah_runtime_debug_slot_tap_action(right_alt_pos) == ARROW_MODE_LOCK);
        CHECK(noah_runtime_debug_slot_held_action_keycode(right_alt_pos) == KC_NO);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);

        if (release_gui_first) {
            test_release_resolved(gui_pos);
            CHECK(noah_runtime_debug_slot_owner_keycode(gui_pos) == KC_NO);
            CHECK(noah_runtime_debug_slot_owner_keycode(right_alt_pos) == KC_RIGHT_ALT);
        }

        test_release_resolved(right_alt_pos);
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
        CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
        CHECK(noah_runtime_debug_slot_owner_keycode(right_alt_pos) == KC_NO);
        CHECK(noah_runtime_debug_slot_held_action_keycode(right_alt_pos) == KC_NO);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);

        if (!release_gui_first) {
            test_release_resolved(gui_pos);
        }

        key_runtime_integration_scan();
        CHECK(noah_runtime_debug_active_slot_count() == 0);
        CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
        CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));
        CHECK(fake_mods == 0);
        CHECK(fake_weak_mods == 0);
        CHECK(fake_oneshot_mods == 0);
        CHECK(fake_oneshot_locked_mods == 0);
        CHECK(fake_managed_mods == 0);
        CHECK(fake_physical_mods == 0);

        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, true));
        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, false));

        CHECK(pd_mode_toggle_lock_state(PD_MODE_ARROW));
        CHECK(pd_mode_local_active_snapshot() == 0);
        CHECK(pd_mode_local_locked_snapshot() == 0);
        CHECK(noah_runtime_debug_active_slot_count() == 0);
        CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
    }
}

static void test_assert_nav_overlap_quiescent(keypos_t gui_pos, keypos_t parent_pos) {
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(gui_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_owner_keycode(parent_pos) == KC_NO);
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));
    CHECK(!test_layer_active(LAYER_NAV));
    CHECK(!test_layer_locked(LAYER_NAV));
    CHECK(fake_mods == 0);
    CHECK(fake_weak_mods == 0);
    CHECK(fake_oneshot_mods == 0);
    CHECK(fake_oneshot_locked_mods == 0);
    CHECK(fake_managed_mods == 0);
    CHECK(fake_physical_mods == 0);
    CHECK(auto_mouse_key_tracker == 0);
}

static void test_assert_gui_dragscroll_overlap_quiescent(keypos_t gui_pos, keypos_t parent_pos, keypos_t child_pos) {
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(gui_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_owner_keycode(parent_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(gui_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(parent_pos) == KC_NO);
    CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == KC_NO);
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&(keypos_t){0}));
    CHECK(!test_layer_active(LAYER_NAV));
    CHECK(!test_layer_locked(LAYER_NAV));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(reset_dragscroll_count == 1);
    CHECK(current_cpi == charybdis_get_pointer_default_dpi());
    CHECK(fake_mods == 0);
    CHECK(fake_weak_mods == 0);
    CHECK(fake_oneshot_mods == 0);
    CHECK(fake_oneshot_locked_mods == 0);
    CHECK(fake_managed_mods == 0);
    CHECK(fake_physical_mods == 0);
    CHECK(!dragscroll_enabled);
    CHECK(!sniping_enabled);
    CHECK(auto_mouse_key_tracker == 0);
}

static void test_run_follow_on_nav_tap(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan, keypos_t child_pos) {
    uint16_t previous_tap_count = test_tap_code16_count;

    test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);
    CHECK(test_resolve_keycode(child_pos) == KC_LEFT);

    test_press_resolved(child_pos);
    test_release_resolved(child_pos);

    CHECK(test_tap_code16_count == (uint16_t)(previous_tap_count + 1));
    CHECK(test_last_tap_code16 == KC_LEFT);
    CHECK(test_delayed_action_count == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);

    test_release_resolved(parent_pos);
    key_runtime_integration_scan();
}

static void test_run_follow_on_nav_hold_release(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan, keypos_t child_pos) {
    uint16_t previous_tap_count = test_tap_code16_count;

    test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);
    CHECK(test_resolve_keycode(child_pos) == KC_LEFT);

    test_press_resolved(child_pos);
    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
    test_release_resolved(child_pos);

    CHECK(test_tap_code16_count == (uint16_t)(previous_tap_count + 1));
    CHECK(test_last_tap_code16 == A(KC_LEFT));
    CHECK(test_delayed_action_count == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);

    test_release_resolved(parent_pos);
    key_runtime_integration_scan();
}

static void test_gui_double_tap_hold_keeps_nav_arrow_taps_immediate_across_release_orders(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    keypos_t gui_pos   = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t child_pos = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(child_pos));

    for (uint8_t index = 0; index < ARRAY_SIZE(test_release_orders); index++) {
        const test_release_order_case_t *release_order = &test_release_orders[index];

        test_reset_state();
        (void)release_order->name;

        test_activate_gui_double_tap_alt_hold(gui_pos);
        test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);
        CHECK(test_resolve_keycode(child_pos) == KC_LEFT);

        test_press_resolved(child_pos);

        for (uint8_t release_index = 0; release_index < ARRAY_SIZE(release_order->order); release_index++) {
            switch (release_order->order[release_index]) {
                case TEST_RELEASE_TARGET_CHILD: {
                    uint16_t previous_tap_count     = test_tap_code16_count;
                    uint16_t previous_delayed_count = test_delayed_action_count;

                    test_release_resolved(child_pos);
                    CHECK(test_tap_code16_count == (uint16_t)(previous_tap_count + 1));
                    CHECK(test_last_tap_code16 == KC_LEFT);
                    CHECK(test_delayed_action_count == previous_delayed_count);
                    break;
                }
                case TEST_RELEASE_TARGET_PARENT:
                    test_release_resolved(parent_pos);
                    break;
                case TEST_RELEASE_TARGET_GUI:
                    test_release_resolved(gui_pos);
                    break;
            }
        }

        key_runtime_integration_scan();
        test_assert_nav_overlap_quiescent(gui_pos, parent_pos);

        test_run_follow_on_nav_tap(parent_pos, parent_keycode, requires_threshold_scan, child_pos);
        key_runtime_integration_scan();
        test_assert_nav_overlap_quiescent(gui_pos, parent_pos);
    }
}

static void test_gui_double_tap_hold_keeps_nav_arrow_hold_release_immediate_across_release_orders(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    keypos_t gui_pos   = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t child_pos = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(child_pos));

    for (uint8_t index = 0; index < ARRAY_SIZE(test_release_orders); index++) {
        const test_release_order_case_t *release_order = &test_release_orders[index];

        test_reset_state();
        (void)release_order->name;

        test_activate_gui_double_tap_alt_hold(gui_pos);
        test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);
        CHECK(test_resolve_keycode(child_pos) == KC_LEFT);

        test_press_resolved(child_pos);
        key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);

        for (uint8_t release_index = 0; release_index < ARRAY_SIZE(release_order->order); release_index++) {
            switch (release_order->order[release_index]) {
                case TEST_RELEASE_TARGET_CHILD: {
                    uint16_t previous_tap_count     = test_tap_code16_count;
                    uint16_t previous_delayed_count = test_delayed_action_count;

                    test_release_resolved(child_pos);
                    CHECK(test_tap_code16_count == (uint16_t)(previous_tap_count + 1));
                    CHECK(test_last_tap_code16 == A(KC_LEFT));
                    CHECK(test_delayed_action_count == previous_delayed_count);
                    break;
                }
                case TEST_RELEASE_TARGET_PARENT:
                    test_release_resolved(parent_pos);
                    break;
                case TEST_RELEASE_TARGET_GUI:
                    test_release_resolved(gui_pos);
                    break;
            }
        }

        key_runtime_integration_scan();
        test_assert_nav_overlap_quiescent(gui_pos, parent_pos);

        test_run_follow_on_nav_hold_release(parent_pos, parent_keycode, requires_threshold_scan, child_pos);
        key_runtime_integration_scan();
        test_assert_nav_overlap_quiescent(gui_pos, parent_pos);
    }
}

static void test_gui_double_tap_hold_keeps_nav_arrow_long_hold_immediate_across_release_orders(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    keypos_t gui_pos   = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t child_pos = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(child_pos));

    for (uint8_t index = 0; index < ARRAY_SIZE(test_release_orders); index++) {
        const test_release_order_case_t *release_order = &test_release_orders[index];

        test_reset_state();
        (void)release_order->name;

        test_activate_gui_double_tap_alt_hold(gui_pos);
        test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);
        CHECK(test_resolve_keycode(child_pos) == KC_LEFT);

        test_press_resolved(child_pos);
        key_runtime_integration_advance(&fake_time, CUSTOM_LONGER_HOLD_TERM + 1);
        key_runtime_integration_scan();

        CHECK(test_tap_code16_count == 1);
        CHECK(test_last_tap_code16 == G(KC_LEFT));
        CHECK(test_delayed_action_count == 0);

        for (uint8_t release_index = 0; release_index < ARRAY_SIZE(release_order->order); release_index++) {
            switch (release_order->order[release_index]) {
                case TEST_RELEASE_TARGET_CHILD:
                    test_release_resolved(child_pos);
                    break;
                case TEST_RELEASE_TARGET_PARENT:
                    test_release_resolved(parent_pos);
                    break;
                case TEST_RELEASE_TARGET_GUI:
                    test_release_resolved(gui_pos);
                    break;
            }
        }

        key_runtime_integration_scan();
        CHECK(test_tap_code16_count == 1);
        CHECK(test_last_tap_code16 == G(KC_LEFT));
        CHECK(test_delayed_action_count == 0);
        test_assert_nav_overlap_quiescent(gui_pos, parent_pos);
    }
}

static void test_gui_pending_double_tap_hold_keeps_nav_arrow_hold_release_immediate_across_release_orders(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    keypos_t gui_pos       = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t child_pos     = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);
    keypos_t follow_on_pos = test_find_keypos_on_layer(LAYER_BASE, KC_C);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(child_pos));
    CHECK(test_keypos_valid(follow_on_pos));

    for (uint8_t index = 0; index < ARRAY_SIZE(test_release_orders); index++) {
        const test_release_order_case_t *release_order = &test_release_orders[index];

        test_reset_state();
        (void)release_order->name;

        test_activate_gui_double_tap_alt_hold_pending(gui_pos);
        test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);
        CHECK(test_resolve_keycode(child_pos) == KC_LEFT);

        test_press_resolved(child_pos);
        test_commit_pending_gui_double_tap_alt_hold(gui_pos);

        for (uint8_t release_index = 0; release_index < ARRAY_SIZE(release_order->order); release_index++) {
            switch (release_order->order[release_index]) {
                case TEST_RELEASE_TARGET_CHILD: {
                    uint16_t previous_tap_count = test_tap_code16_count;

                    test_release_resolved(child_pos);
                    CHECK(test_tap_code16_count == (uint16_t)(previous_tap_count + 1));
                    CHECK(test_last_tap_code16 == A(KC_LEFT));
                    CHECK(test_delayed_action_count == 0);
                    break;
                }
                case TEST_RELEASE_TARGET_PARENT:
                    test_release_resolved(parent_pos);
                    break;
                case TEST_RELEASE_TARGET_GUI:
                    test_release_resolved(gui_pos);
                    break;
            }
        }

        key_runtime_integration_scan();
        test_assert_nav_overlap_quiescent(gui_pos, parent_pos);

        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, true));
        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, false));
        test_assert_nav_overlap_quiescent(gui_pos, parent_pos);
    }
}

static void test_gui_pending_double_tap_hold_with_nav_dragscroll_keeps_runtime_quiescent(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    keypos_t gui_pos        = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t dragscroll_pos = test_find_keypos_on_layer(LAYER_NAV, DRAGSCROLL);
    keypos_t follow_on_pos  = test_find_keypos_on_layer(LAYER_BASE, KC_C);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(parent_pos));
    CHECK(test_keypos_valid(dragscroll_pos));
    CHECK(test_keypos_valid(follow_on_pos));

    for (uint8_t index = 0; index < ARRAY_SIZE(test_release_orders); index++) {
        const test_release_order_case_t *release_order = &test_release_orders[index];

        test_reset_state();
        (void)release_order->name;

        test_activate_gui_double_tap_alt_hold_pending(gui_pos);
        test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);
        CHECK(test_resolve_keycode(dragscroll_pos) == DRAGSCROLL);

        test_press_resolved(dragscroll_pos);
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
        CHECK(noah_runtime_debug_slot_owner_keycode(dragscroll_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_slot_held_action_keycode(dragscroll_pos) == DRAGSCROLL);

        test_commit_pending_gui_double_tap_alt_hold(gui_pos);
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);

        for (uint8_t release_index = 0; release_index < ARRAY_SIZE(release_order->order); release_index++) {
            switch (release_order->order[release_index]) {
                case TEST_RELEASE_TARGET_CHILD:
                    test_release_resolved(dragscroll_pos);
                    break;
                case TEST_RELEASE_TARGET_PARENT:
                    test_release_resolved(parent_pos);
                    break;
                case TEST_RELEASE_TARGET_GUI:
                    test_release_resolved(gui_pos);
                    break;
            }
        }

        key_runtime_integration_scan();
        test_assert_gui_dragscroll_overlap_quiescent(gui_pos, parent_pos, dragscroll_pos);

        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, true));
        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, false));
        test_assert_gui_dragscroll_overlap_quiescent(gui_pos, parent_pos, dragscroll_pos);
    }
}

static void test_gui_double_tap_hold_with_raw_nav_lt_keeps_arrow_taps_immediate(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);

    CHECK(test_keypos_valid(nav_hold_pos));
    test_gui_double_tap_hold_keeps_nav_arrow_taps_immediate_across_release_orders(nav_hold_pos, nav_hold_keycode, false);
    test_gui_double_tap_hold_keeps_nav_arrow_hold_release_immediate_across_release_orders(nav_hold_pos, nav_hold_keycode, false);
    test_gui_double_tap_hold_keeps_nav_arrow_long_hold_immediate_across_release_orders(nav_hold_pos, nav_hold_keycode, false);
}

static void test_gui_double_tap_hold_with_right_thumb_nav_hold_keeps_arrow_taps_immediate(void) {
    keypos_t right_thumb_pos     = test_right_thumb_pos();
    uint16_t right_thumb_keycode = test_keycode_at(LAYER_BASE, right_thumb_pos);

    CHECK(right_thumb_keycode != KC_TRNS);
    test_gui_double_tap_hold_keeps_nav_arrow_taps_immediate_across_release_orders(right_thumb_pos, right_thumb_keycode, true);
    test_gui_double_tap_hold_keeps_nav_arrow_hold_release_immediate_across_release_orders(right_thumb_pos, right_thumb_keycode, true);
    test_gui_double_tap_hold_keeps_nav_arrow_long_hold_immediate_across_release_orders(right_thumb_pos, right_thumb_keycode, true);
}

static void test_gui_double_tap_hold_with_repeated_nav_arrow_taps_stays_quiescent(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    keypos_t gui_pos   = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t child_pos = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(parent_pos));
    CHECK(test_keypos_valid(child_pos));

    test_reset_state();
    test_activate_gui_double_tap_alt_hold(gui_pos);
    test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);

    for (uint8_t iteration = 0; iteration < 16; iteration++) {
        uint16_t previous_tap_count = test_tap_code16_count;

        CHECK(test_resolve_keycode(child_pos) == KC_LEFT);
        test_press_resolved(child_pos);
        test_release_resolved(child_pos);
        CHECK(test_tap_code16_count == (uint16_t)(previous_tap_count + 1));
        CHECK(test_last_tap_code16 == KC_LEFT);
        CHECK(noah_runtime_debug_slot_owner_keycode(child_pos) == KC_NO);
        CHECK(noah_runtime_debug_slot_held_action_keycode(child_pos) == KC_NO);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);

        key_runtime_integration_advance(&fake_time, 10);
        key_runtime_integration_scan();
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
    }

    test_release_resolved(parent_pos);
    test_release_resolved(gui_pos);
    key_runtime_integration_scan();
    test_assert_nav_overlap_quiescent(gui_pos, parent_pos);
}

static void test_gui_double_tap_hold_with_raw_nav_lt_repeated_arrow_taps_stays_quiescent(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);

    CHECK(test_keypos_valid(nav_hold_pos));
    test_gui_double_tap_hold_with_repeated_nav_arrow_taps_stays_quiescent(nav_hold_pos, nav_hold_keycode, false);
}

static void test_gui_double_tap_hold_with_right_thumb_repeated_arrow_taps_stays_quiescent(void) {
    keypos_t right_thumb_pos     = test_right_thumb_pos();
    uint16_t right_thumb_keycode = test_keycode_at(LAYER_BASE, right_thumb_pos);

    CHECK(right_thumb_keycode != KC_TRNS);
    test_gui_double_tap_hold_with_repeated_nav_arrow_taps_stays_quiescent(right_thumb_pos, right_thumb_keycode, true);
}

static void test_gui_pending_double_tap_hold_with_raw_nav_lt_keeps_arrow_hold_release_immediate(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);

    CHECK(test_keypos_valid(nav_hold_pos));
    test_gui_pending_double_tap_hold_keeps_nav_arrow_hold_release_immediate_across_release_orders(nav_hold_pos, nav_hold_keycode, false);
}

static void test_gui_pending_double_tap_hold_keeps_arrow_tap_immediate(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    keypos_t gui_pos       = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t child_pos     = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);
    keypos_t follow_on_pos = test_find_keypos_on_layer(LAYER_BASE, KC_C);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(parent_pos));
    CHECK(test_keypos_valid(child_pos));
    CHECK(test_keypos_valid(follow_on_pos));

    for (uint8_t index = 0; index < 2; index++) {
        bool     release_parent_first = index != 0;
        uint16_t previous_tap_count;

        test_reset_state();

        test_activate_gui_double_tap_alt_hold_pending(gui_pos);
        test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);
        CHECK(test_resolve_keycode(child_pos) == KC_LEFT);

        previous_tap_count = test_tap_code16_count;
        test_press_resolved(child_pos);
        test_release_resolved(child_pos);
        CHECK(test_tap_code16_count == (uint16_t)(previous_tap_count + 1));
        CHECK(test_last_tap_code16 == KC_LEFT);
        CHECK(test_delayed_action_count == 0);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);

        test_commit_pending_gui_double_tap_alt_hold(gui_pos);

        if (release_parent_first) {
            test_release_resolved(parent_pos);
            test_release_resolved(gui_pos);
        } else {
            test_release_resolved(gui_pos);
            test_release_resolved(parent_pos);
        }

        key_runtime_integration_scan();
        test_assert_nav_overlap_quiescent(gui_pos, parent_pos);

        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, true));
        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, false));
        test_assert_nav_overlap_quiescent(gui_pos, parent_pos);
    }
}

static void test_gui_pending_double_tap_hold_with_raw_nav_lt_keeps_arrow_tap_immediate(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);

    CHECK(test_keypos_valid(nav_hold_pos));
    test_gui_pending_double_tap_hold_keeps_arrow_tap_immediate(nav_hold_pos, nav_hold_keycode, false);
}

static void test_gui_pending_double_tap_hold_with_right_thumb_nav_hold_keeps_arrow_tap_immediate(void) {
    keypos_t right_thumb_pos     = test_right_thumb_pos();
    uint16_t right_thumb_keycode = test_keycode_at(LAYER_BASE, right_thumb_pos);

    CHECK(right_thumb_keycode != KC_TRNS);
    test_gui_pending_double_tap_hold_keeps_arrow_tap_immediate(right_thumb_pos, right_thumb_keycode, true);
}

static void test_gui_double_tap_hold_with_nav_dragscroll_keeps_runtime_quiescent(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    keypos_t gui_pos        = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t dragscroll_pos = test_find_keypos_on_layer(LAYER_NAV, DRAGSCROLL);
    keypos_t follow_on_pos  = test_find_keypos_on_layer(LAYER_BASE, KC_C);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(parent_pos));
    CHECK(test_keypos_valid(dragscroll_pos));
    CHECK(test_keypos_valid(follow_on_pos));

    for (uint8_t index = 0; index < ARRAY_SIZE(test_release_orders); index++) {
        const test_release_order_case_t *release_order = &test_release_orders[index];

        test_reset_state();
        (void)release_order->name;

        test_activate_gui_double_tap_alt_hold(gui_pos);
        test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);
        CHECK(test_resolve_keycode(dragscroll_pos) == DRAGSCROLL);

        test_press_resolved(dragscroll_pos);
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
        CHECK(pd_mode_local_locked_snapshot() == 0);
        CHECK(noah_runtime_debug_slot_owner_keycode(dragscroll_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_slot_held_action_keycode(dragscroll_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);
        CHECK(reset_dragscroll_count == 0);

        key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
        key_runtime_integration_scan();
        CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
        CHECK(noah_runtime_debug_slot_owner_keycode(dragscroll_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_slot_held_action_keycode(dragscroll_pos) == DRAGSCROLL);
        CHECK(noah_runtime_debug_deferred_release_count() == 0);

        for (uint8_t release_index = 0; release_index < ARRAY_SIZE(release_order->order); release_index++) {
            switch (release_order->order[release_index]) {
                case TEST_RELEASE_TARGET_CHILD:
                    test_release_resolved(dragscroll_pos);
                    break;
                case TEST_RELEASE_TARGET_PARENT:
                    test_release_resolved(parent_pos);
                    break;
                case TEST_RELEASE_TARGET_GUI:
                    test_release_resolved(gui_pos);
                    break;
            }
        }

        key_runtime_integration_scan();
        test_assert_gui_dragscroll_overlap_quiescent(gui_pos, parent_pos, dragscroll_pos);

        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, true));
        CHECK(key_runtime_integration_process_record(KC_C, follow_on_pos, false));
        test_assert_gui_dragscroll_overlap_quiescent(gui_pos, parent_pos, dragscroll_pos);
    }
}

static void test_gui_double_tap_hold_with_raw_nav_lt_keeps_dragscroll_immediate(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);

    CHECK(test_keypos_valid(nav_hold_pos));
    test_gui_double_tap_hold_with_nav_dragscroll_keeps_runtime_quiescent(nav_hold_pos, nav_hold_keycode, false);
}

static void test_gui_double_tap_hold_with_right_thumb_nav_hold_keeps_dragscroll_immediate(void) {
    keypos_t right_thumb_pos     = test_right_thumb_pos();
    uint16_t right_thumb_keycode = test_keycode_at(LAYER_BASE, right_thumb_pos);

    CHECK(right_thumb_keycode != KC_TRNS);
    test_gui_double_tap_hold_with_nav_dragscroll_keeps_runtime_quiescent(right_thumb_pos, right_thumb_keycode, true);
}

static void test_gui_pending_double_tap_hold_with_raw_nav_lt_keeps_dragscroll_immediate(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);

    CHECK(test_keypos_valid(nav_hold_pos));
    test_gui_pending_double_tap_hold_with_nav_dragscroll_keeps_runtime_quiescent(nav_hold_pos, nav_hold_keycode, false);
}

static void test_gui_pending_double_tap_hold_with_right_thumb_nav_hold_keeps_arrow_hold_release_immediate(void) {
    keypos_t right_thumb_pos     = test_right_thumb_pos();
    uint16_t right_thumb_keycode = test_keycode_at(LAYER_BASE, right_thumb_pos);

    CHECK(right_thumb_keycode != KC_TRNS);
    test_gui_pending_double_tap_hold_keeps_nav_arrow_hold_release_immediate_across_release_orders(right_thumb_pos, right_thumb_keycode, true);
}

static void test_nav_dragscroll_hold_keeps_arrow_taps_immediate(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    keypos_t dragscroll_pos = test_find_keypos_on_layer(LAYER_NAV, DRAGSCROLL);
    keypos_t child_pos      = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);
    uint16_t previous_tap_count;

    CHECK(test_keypos_valid(parent_pos));
    CHECK(test_keypos_valid(dragscroll_pos));
    CHECK(test_keypos_valid(child_pos));

    test_reset_state();
    test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);

    test_press_resolved(dragscroll_pos);
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
    CHECK(noah_runtime_debug_slot_owner_keycode(dragscroll_pos) == DRAGSCROLL);
    CHECK(noah_runtime_debug_slot_held_action_keycode(dragscroll_pos) == DRAGSCROLL);

    previous_tap_count = test_tap_code16_count;
    test_press_resolved(child_pos);
    test_release_resolved(child_pos);
    CHECK(test_tap_code16_count == (uint16_t)(previous_tap_count + 1));
    CHECK(test_last_tap_code16 == KC_LEFT);
    CHECK(test_delayed_action_count == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);

    test_release_resolved(dragscroll_pos);
    test_release_resolved(parent_pos);
    key_runtime_integration_scan();
    test_assert_dragscroll_overlap_quiescent(parent_pos, dragscroll_pos);
}

static void test_raw_nav_dragscroll_hold_keeps_arrow_taps_immediate(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);

    CHECK(test_keypos_valid(nav_hold_pos));
    test_nav_dragscroll_hold_keeps_arrow_taps_immediate(nav_hold_pos, nav_hold_keycode, false);
}

static void test_right_thumb_dragscroll_hold_keeps_arrow_taps_immediate(void) {
    keypos_t right_thumb_pos     = test_right_thumb_pos();
    uint16_t right_thumb_keycode = test_keycode_at(LAYER_BASE, right_thumb_pos);

    CHECK(right_thumb_keycode != KC_TRNS);
    test_nav_dragscroll_hold_keeps_arrow_taps_immediate(right_thumb_pos, right_thumb_keycode, true);
}

static void test_gui_double_tap_hold_with_nav_dragscroll_active_keeps_arrow_taps_immediate(keypos_t parent_pos, uint16_t parent_keycode, bool requires_threshold_scan) {
    keypos_t gui_pos        = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t dragscroll_pos = test_find_keypos_on_layer(LAYER_NAV, DRAGSCROLL);
    keypos_t child_pos      = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);
    uint16_t previous_tap_count;

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(parent_pos));
    CHECK(test_keypos_valid(dragscroll_pos));
    CHECK(test_keypos_valid(child_pos));

    test_reset_state();
    test_activate_gui_double_tap_alt_hold(gui_pos);
    test_activate_nav_parent_hold(parent_pos, parent_keycode, requires_threshold_scan);

    test_press_resolved(dragscroll_pos);
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_DRAGSCROLL);
    CHECK(noah_runtime_debug_slot_owner_keycode(dragscroll_pos) == DRAGSCROLL);
    CHECK(noah_runtime_debug_slot_held_action_keycode(dragscroll_pos) == DRAGSCROLL);

    previous_tap_count = test_tap_code16_count;
    test_press_resolved(child_pos);
    test_release_resolved(child_pos);
    CHECK(test_tap_code16_count == (uint16_t)(previous_tap_count + 1));
    CHECK(test_last_tap_code16 == KC_LEFT);
    CHECK(test_delayed_action_count == 0);
    CHECK(noah_runtime_debug_deferred_release_count() == 0);

    test_release_resolved(dragscroll_pos);
    test_release_resolved(parent_pos);
    test_release_resolved(gui_pos);
    key_runtime_integration_scan();
    test_assert_gui_dragscroll_overlap_quiescent(gui_pos, parent_pos, dragscroll_pos);
}

static void test_gui_double_tap_hold_with_raw_nav_dragscroll_active_keeps_arrow_taps_immediate(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);

    CHECK(test_keypos_valid(nav_hold_pos));
    test_gui_double_tap_hold_with_nav_dragscroll_active_keeps_arrow_taps_immediate(nav_hold_pos, nav_hold_keycode, false);
}

static void test_gui_double_tap_hold_with_right_thumb_dragscroll_active_keeps_arrow_taps_immediate(void) {
    keypos_t right_thumb_pos     = test_right_thumb_pos();
    uint16_t right_thumb_keycode = test_keycode_at(LAYER_BASE, right_thumb_pos);

    CHECK(right_thumb_keycode != KC_TRNS);
    test_gui_double_tap_hold_with_nav_dragscroll_active_keeps_arrow_taps_immediate(right_thumb_pos, right_thumb_keycode, true);
}

static void test_gui_pending_double_tap_hold_with_right_thumb_nav_hold_keeps_dragscroll_immediate(void) {
    keypos_t right_thumb_pos     = test_right_thumb_pos();
    uint16_t right_thumb_keycode = test_keycode_at(LAYER_BASE, right_thumb_pos);

    CHECK(right_thumb_keycode != KC_TRNS);
    test_gui_pending_double_tap_hold_with_nav_dragscroll_keeps_runtime_quiescent(right_thumb_pos, right_thumb_keycode, true);
}

static void test_key_runtime_core_raw_nav_dragscroll_shadow_scenario(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);
    keypos_t       dragscroll_pos   = test_find_keypos_on_layer(LAYER_NAV, DRAGSCROLL);

    CHECK(test_keypos_valid(nav_hold_pos));
    CHECK(test_keypos_valid(dragscroll_pos));

    test_activate_nav_parent_hold(nav_hold_pos, nav_hold_keycode, false);
    CHECK(test_resolve_keycode(dragscroll_pos) == DRAGSCROLL);

    test_press_resolved(dragscroll_pos);
    key_runtime_integration_advance(&fake_time, CUSTOM_TAP_HOLD_TERM + 1);
    key_runtime_integration_scan();
    test_release_resolved(dragscroll_pos);
    test_release_resolved(nav_hold_pos);
    key_runtime_integration_scan();

    test_assert_dragscroll_overlap_quiescent(nav_hold_pos, dragscroll_pos);
}

static void test_key_runtime_core_gui_alt_repeated_nav_shadow_scenario(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t       gui_pos          = test_find_keypos_on_layer(LAYER_BASE, KC_LEFT_GUI);
    keypos_t       nav_hold_pos     = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);
    keypos_t       child_pos        = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);

    CHECK(test_keypos_valid(gui_pos));
    CHECK(test_keypos_valid(nav_hold_pos));
    CHECK(test_keypos_valid(child_pos));

    test_activate_gui_double_tap_alt_hold(gui_pos);
    test_activate_nav_parent_hold(nav_hold_pos, nav_hold_keycode, false);

    for (uint8_t iteration = 0; iteration < 4u; iteration++) {
        uint16_t previous_tap_count = test_tap_code16_count;

        CHECK(test_resolve_keycode(child_pos) == KC_LEFT);
        test_press_resolved(child_pos);
        test_release_resolved(child_pos);
        CHECK(test_tap_code16_count == (uint16_t)(previous_tap_count + 1u));
        CHECK(test_last_tap_code16 == KC_LEFT);

        key_runtime_integration_advance(&fake_time, 10u);
        key_runtime_integration_scan();
        CHECK(noah_runtime_debug_deferred_release_count() == 0u);
    }

    test_release_resolved(nav_hold_pos);
    test_release_resolved(gui_pos);
    key_runtime_integration_scan();

    test_assert_nav_overlap_quiescent(gui_pos, nav_hold_pos);
}

static void test_key_runtime_core_shadow_replays_raw_nav_dragscroll_overlap(void) {
    test_shadow_replay_scenario(test_key_runtime_core_raw_nav_dragscroll_shadow_scenario);
}

static void test_key_runtime_core_shadow_replays_gui_alt_repeated_nav_overlap(void) {
    test_shadow_replay_scenario(test_key_runtime_core_gui_alt_repeated_nav_shadow_scenario);
}

int main(void) {
    test_left_thumb_double_tap_hold_toggles_num_layer();
    test_right_thumb_double_tap_hold_toggles_num_layer();
    test_thumb_double_tap_hold_with_intermediate_scan_toggles_num_layer_once_per_cycle();
    test_left_thumb_double_tap_hold_escape_feedback_sequence();
    test_left_thumb_double_tap_hold_escape_release_crossing_threshold_pulses_branch();
    test_left_thumb_double_tap_hold_escape_release_during_branch_keeps_hold_feedback_pulse();
    test_left_thumb_double_tap_long_hold_num_feedback_replaces_branch();
    test_left_and_right_thumb_single_taps_keep_independent_pending_chains();
    test_number_key_hold_only_tap_feedback_stays_quiet();
    test_number_key_hold_only_repeated_taps_do_not_enter_branch_feedback();
    test_right_thumb_triple_tap_flushes_next_track_after_timeout();
    test_right_thumb_triple_tap_long_hold_registers_next_track_hold();
    test_right_thumb_quadruple_tap_dispatches_previous_track();
    test_pointer_pinch_double_tap_queues_zoom_chord();
    test_pointer_pinch_double_tap_salvos_queue_zoom_chord_cleanly();
    test_pointer_pinch_exact_third_tap_defers_zoom_chord_until_release();
    test_pointer_volume_exact_third_tap_defers_mute_until_release();
    test_click_spam_combo_uses_authored_combo_owner();
    test_cmd_combo_double_tap_hold_uses_stable_owner_across_press_order();
    test_right_nav_layer_hold_dispatches_nav_taps_immediately();
    test_right_thumb_hold_dispatches_nav_taps_immediately();
    test_raw_nav_layer_hold_enters_dragscroll_mode_cleanly();
    test_right_thumb_nav_hold_enters_dragscroll_mode_cleanly();
    test_nav_dragscroll_repeated_quick_taps_stay_quiescent();
    test_pointer_dragscroll_repeated_quick_taps_stay_quiescent();
    test_same_locked_nav_dragscroll_press_unlocks_and_holds_until_release();
    test_gui_triple_tap_osm_flushes_before_next_plain_key();
    test_raw_nav_dragscroll_hold_keeps_arrow_taps_immediate();
    test_right_thumb_dragscroll_hold_keeps_arrow_taps_immediate();
    test_gui_double_tap_hold_with_right_alt_arrow_mode_lock_stays_usable();
    test_gui_double_tap_hold_with_raw_nav_lt_keeps_arrow_taps_immediate();
    test_gui_double_tap_hold_with_right_thumb_nav_hold_keeps_arrow_taps_immediate();
    test_gui_double_tap_hold_with_raw_nav_lt_repeated_arrow_taps_stays_quiescent();
    test_gui_double_tap_hold_with_right_thumb_repeated_arrow_taps_stays_quiescent();
    test_gui_double_tap_hold_with_raw_nav_dragscroll_active_keeps_arrow_taps_immediate();
    test_gui_double_tap_hold_with_right_thumb_dragscroll_active_keeps_arrow_taps_immediate();
    test_gui_pending_double_tap_hold_with_raw_nav_lt_keeps_arrow_tap_immediate();
    test_gui_pending_double_tap_hold_with_raw_nav_lt_keeps_arrow_hold_release_immediate();
    test_gui_pending_double_tap_hold_with_right_thumb_nav_hold_keeps_arrow_tap_immediate();
    test_gui_pending_double_tap_hold_with_right_thumb_nav_hold_keeps_arrow_hold_release_immediate();
    test_gui_double_tap_hold_with_raw_nav_lt_keeps_dragscroll_immediate();
    test_gui_double_tap_hold_with_right_thumb_nav_hold_keeps_dragscroll_immediate();
    test_gui_pending_double_tap_hold_with_raw_nav_lt_keeps_dragscroll_immediate();
    test_gui_pending_double_tap_hold_with_right_thumb_nav_hold_keeps_dragscroll_immediate();
    test_key_runtime_core_shadow_replays_raw_nav_dragscroll_overlap();
    test_key_runtime_core_shadow_replays_gui_alt_repeated_nav_overlap();

    puts("real_profile_thumb_layer_lock integration tests passed");
    return 0;
}
