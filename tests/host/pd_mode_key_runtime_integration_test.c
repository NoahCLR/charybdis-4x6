#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/key/runtime/key_runtime_process.h"
#include "users/noah/lib/key/runtime/key_runtime_state.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/state/runtime/runtime_shared_state.h"
#include "users/noah/noah_runtime.h"

const key_behavior_t key_behaviors[1]   = {0};
const uint8_t        key_behavior_count = 0;

static uint16_t fake_time;
static uint16_t current_cpi;
static uint16_t default_dpi;
static uint8_t  split_sync_count;
static uint8_t  reset_volume_count;

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

static void test_reset_state(void) {
    noah_runtime_shared_state                         = (runtime_shared_state_t){0};
    *key_runtime_slot_for_position(test_keypos(1, 2)) = (active_key_state_t)ACTIVE_KEY_STATE_INIT;

    fake_time          = 1000;
    current_cpi        = 0;
    default_dpi        = 900;
    split_sync_count   = 0;
    reset_volume_count = 0;
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

bool action_dispatch_is_layer_lock(uint16_t action) {
    (void)action;
    return false;
}

bool action_dispatch_is_raw_qmk_layer_action(uint16_t action) {
    return IS_QK_MOMENTARY(action) || IS_QK_LAYER_TAP(action);
}

bool action_dispatch_is_macro(uint16_t action) {
    (void)action;
    return false;
}

bool action_dispatch_is_qmk_behavior_keycode(uint16_t action) {
    (void)action;
    return false;
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    (void)action;
    (void)policy;
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

void noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    (void)keycode;
    (void)pressed;
}

void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count) {
    (void)keycode;
    (void)pressed;
    (void)tap_count;
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

void keyboard_mod_ownership_register_mods(uint8_t mods) {
    (void)mods;
}

void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    (void)mods;
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    (void)keycode;
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    (void)keycode;
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
    return (keyboard_mod_state_t){0};
}

void keyboard_mod_state_apply(keyboard_mod_state_t state) {
    (void)state;
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
    (void)action;
    (void)mods;
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

static void test_plain_pd_mode_key_activates_and_deactivates_through_process_record(void) {
    keypos_t            key_pos        = test_keypos(1, 2);
    keyrecord_t         press_record   = test_record(key_pos, true);
    keyrecord_t         release_record = test_record(key_pos, false);
    active_key_state_t *slot           = key_runtime_slot_for_position(key_pos);

    test_reset_state();

    CHECK(!key_behavior_lookup(VOLUME_MODE).config);
    CHECK(pd_mode_for_keycode(VOLUME_MODE) == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active_snapshot() == 0);

    CHECK(!noah_process_record_user(VOLUME_MODE, &press_record));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(slot->owner.keycode == VOLUME_MODE);
    CHECK(slot->lifecycle.held_action_keycode == VOLUME_MODE);
    CHECK(split_sync_count >= 1);

    fake_time += 10;

    CHECK(!noah_process_record_user(VOLUME_MODE, &release_record));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(!pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(slot->owner.keycode == KC_NO);
    CHECK(slot->lifecycle.held_action_keycode == KC_NO);
    CHECK(reset_volume_count == 1);
    CHECK(current_cpi == default_dpi);
}

int main(void) {
    test_plain_pd_mode_key_activates_and_deactivates_through_process_record();

    puts("pd_mode_key_runtime_integration host tests passed");
    return 0;
}
