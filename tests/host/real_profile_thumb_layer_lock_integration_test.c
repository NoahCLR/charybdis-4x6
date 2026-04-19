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
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/state/ownership/layer_ownership.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"
#include "users/noah/lib/state/runtime/runtime_reset.h"
#include "users/noah/noah_keymap.h"
#include "users/noah/noah_runtime.h"

layer_state_t layer_state = 0;

static uint16_t fake_time;
static uint16_t test_tap_code16_count;
static uint16_t test_last_tap_code16;
static uint16_t test_delayed_action_count;
static uint16_t test_last_delayed_action;

typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
} test_held_action_binding_t;

static test_held_action_binding_t test_held_actions[MATRIX_ROWS * MATRIX_COLS];

extern const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS];

#define NOAH_PD_MODE_TEST_ROW(name, mode_keycode, handler, key_handler, reset, dpi, mode_traits, lifecycle) [PD_MODE_INDEX_##name] = {.mode_flag = PD_MODE_##name, .keycode = (mode_keycode), .lock_action = mode_keycode##_LOCK, .traits = (mode_traits)},
const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {NOAH_PD_MODE_LIST(NOAH_PD_MODE_TEST_ROW)};
#undef NOAH_PD_MODE_TEST_ROW

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

static void test_press_resolved(keypos_t key_pos) {
    uint16_t keycode = test_resolve_keycode(key_pos);

    CHECK(!key_runtime_integration_process_record(keycode, key_pos, true));
}

static void test_release_resolved(keypos_t key_pos) {
    uint16_t keycode = test_resolve_keycode(key_pos);

    CHECK(!key_runtime_integration_process_record(keycode, key_pos, false));
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

static void test_reset_state(void) {
    fake_time                 = 1000;
    test_tap_code16_count     = 0;
    test_last_tap_code16      = KC_NO;
    test_delayed_action_count = 0;
    test_last_delayed_action  = KC_NO;
    memset(test_held_actions, 0, sizeof(test_held_actions));
    noah_runtime_reset_for_test();
    layer_state = test_layer_mask(LAYER_BASE);
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

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

void layer_on(uint8_t layer) {
    layer_state |= (layer_state_t)1u << layer;
}

void layer_off(uint8_t layer) {
    layer_state &= ~((layer_state_t)1u << layer);
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return keymaps[layer_num][row][column];
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
    (void)keycode;
}

void unregister_code(uint8_t keycode) {
    (void)keycode;
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

bool owned_keycode_register(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool owned_keycode_unregister(uint16_t keycode) {
    (void)keycode;
    return false;
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    (void)keycode;
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    (void)keycode;
}

void keyboard_mod_ownership_register_mods(uint8_t mods) {
    (void)mods;
}

void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    (void)mods;
}

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    (void)mods;
    return 0;
}

void keyboard_mod_state_apply(keyboard_mod_state_t state) {
    (void)state;
}

keyboard_mod_state_t keyboard_mod_state_suspend(void) {
    return (keyboard_mod_state_t){0};
}

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    for (uint8_t index = 0; index < PD_MODE_COUNT; index++) {
        if (pd_modes[index].lock_action == action) {
            return &pd_modes[index];
        }
    }

    return NULL;
}

bool is_pd_mode_lock_action(uint16_t action) {
    return pd_mode_lock_action_lookup(action) != NULL;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t index = 0; index < PD_MODE_COUNT; index++) {
        if (pd_modes[index].keycode == keycode) {
            return pd_modes[index].mode_flag;
        }
    }

    return 0;
}

bool pd_mode_handle_keycode_press(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool pd_mode_handle_keycode_release(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool pd_mode_local_locked(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

bool pd_any_mode_active(void) {
    return false;
}

bool pd_any_active_mode_has_trait(pd_mode_traits_t trait) {
    (void)trait;
    return false;
}

bool pd_mode_has_trait(pd_mode_mask_t mode, pd_mode_traits_t trait) {
    (void)mode;
    (void)trait;
    return false;
}

bool charybdis_get_pointer_dragscroll_enabled(void) {
    return false;
}

bool charybdis_get_pointer_sniping_enabled(void) {
    return false;
}

uint16_t charybdis_get_pointer_default_dpi(void) {
    return 800;
}

void charybdis_set_pointer_dragscroll_enabled(bool enabled) {
    (void)enabled;
}

void charybdis_set_pointer_sniping_enabled(bool enabled) {
    (void)enabled;
}

void pointing_device_set_cpi(uint16_t cpi) {
    (void)cpi;
}

bool is_keyboard_master(void) {
    return true;
}

uint16_t auto_mouse_get_time_elapsed(void) {
    return 0;
}

bool is_auto_mouse_active(void) {
    return false;
}

void auto_mouse_keyevent(bool pressed) {
    (void)pressed;
}

uint8_t auto_mouse_key_tracker(void) {
    return 0;
}

bool auto_mouse_toggle_enabled(void) {
    return false;
}

void auto_mouse_layer_off(void) {}

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

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    test_delayed_action_count++;
    test_last_delayed_action = action;
    (void)mods;
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
        action = test_held_actions[slot].action;
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

    uint16_t action = test_held_actions[slot].action;
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

void held_repeat_tick(void) {}

bool held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    int16_t slot = test_find_held_action_slot(key_pos);

    return slot >= 0 && test_held_actions[slot].action == action;
}

void key_feedback_pulse_arm(bool long_hold_level) {
    (void)long_hold_level;
}

uint8_t key_feedback_pack(void) {
    return 0;
}

uint8_t key_feedback_preview_layer(void) {
    return UINT8_MAX;
}

void split_runtime_sync(void) {}

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

static void test_right_nav_layer_hold_dispatches_nav_taps_immediately(void) {
    const uint16_t nav_hold_keycode = LT(LAYER_NAV, KC_SLSH);
    keypos_t        nav_hold_pos    = test_find_keypos_on_layer(LAYER_BASE, nav_hold_keycode);
    keypos_t        nav_left_pos    = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);

    test_reset_state();

    CHECK(test_keypos_valid(nav_hold_pos));
    CHECK(test_keypos_valid(nav_left_pos));
    CHECK(test_resolve_keycode(nav_hold_pos) == nav_hold_keycode);

    test_press_resolved(nav_hold_pos);
    CHECK(test_layer_active(LAYER_NAV));

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
}

static void test_right_thumb_hold_dispatches_nav_taps_immediately(void) {
    keypos_t  right_thumb_pos     = test_right_thumb_pos();
    keypos_t  nav_left_pos        = test_find_keypos_on_layer(LAYER_NAV, KC_LEFT);
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

int main(void) {
    test_left_thumb_double_tap_hold_toggles_num_layer();
    test_right_thumb_double_tap_hold_toggles_num_layer();
    test_thumb_double_tap_hold_with_intermediate_scan_toggles_num_layer_once_per_cycle();
    test_right_nav_layer_hold_dispatches_nav_taps_immediately();
    test_right_thumb_hold_dispatches_nav_taps_immediately();

    puts("real_profile_thumb_layer_lock integration tests passed");
    return 0;
}
