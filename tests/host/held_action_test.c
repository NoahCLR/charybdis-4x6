#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/action/custom_key_shortcut.h"
#include "users/noah/noah_keymap_ids.h"
#include "users/noah/lib/key/behavior/key_behavior.h"
#include "users/noah/lib/key/ownership/held_action.h"
#include "users/noah/lib/key/ownership/held_repeat.h"
#include "users/noah/lib/state/shared/runtime_reset.h"

enum {
    TEST_SHARED_ACTION = SAFE_RANGE + 0x40,
    TEST_SECOND_ACTION = SAFE_RANGE + 0x41,
};

#define TEST_PER_KEY_ACTION MO(2)
#define TEST_PRESS_ONLY_ACTION LOCK_LAYER(3)

typedef struct {
    keypos_t key_pos;
    uint16_t action;
} action_call_t;

typedef struct {
    uint16_t action;
    uint8_t  row;
    uint8_t  col;
} tap_call_t;

typedef struct {
    uint16_t action;
    bool     pressed;
} pointer_action_call_t;

static action_call_t         press_calls[16];
static action_call_t         release_calls[16];
static tap_call_t            tap_calls[32];
static pointer_action_call_t pointer_action_calls[16];
static uint8_t               press_call_count;
static uint8_t               release_call_count;
static uint8_t               tap_call_count;
static uint8_t               pointer_action_call_count;
static uint16_t              mod_register_calls[16];
static uint16_t              mod_unregister_calls[16];
static uint8_t               mod_register_count;
static uint8_t               mod_unregister_count;
static uint16_t              owned_acquire_calls[16];
static uint16_t              owned_release_calls[16];
static uint8_t               owned_acquire_count;
static uint8_t               owned_release_count;
static int16_t               owned_failed_acquire_keycode;
static owned_keycode_lease_t owned_part_calls[16];
static uint8_t               owned_part_count;
static uint8_t               owned_part_release_mods[16];
static uint8_t               owned_part_release_count;
// The live shortcuts, as the settings runtime would report them.
static noah_custom_key_shortcut_t test_shortcuts[CUSTOM_KEY_SLOT_COUNT];
static uint16_t              fake_time;
static uint8_t               timer_read_count;
layer_state_t                layer_state;

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

static void test_reset_stubs(void) {
    noah_runtime_reset_for_test();

    press_call_count             = 0;
    release_call_count           = 0;
    tap_call_count               = 0;
    pointer_action_call_count    = 0;
    mod_register_count           = 0;
    mod_unregister_count         = 0;
    owned_acquire_count          = 0;
    owned_release_count          = 0;
    owned_failed_acquire_keycode = -1;
    owned_part_count             = 0;
    owned_part_release_count     = 0;
    memset(test_shortcuts, 0, sizeof(test_shortcuts));
    fake_time                    = 1000;
    timer_read_count             = 0;
}

bool is_pointing_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

pointing_mode_mask_t pointing_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

void noah_action_press(keypos_t key_pos, uint16_t action) {
    CHECK(press_call_count < ARRAY_SIZE(press_calls));
    press_calls[press_call_count++] = (action_call_t){
        .key_pos = key_pos,
        .action  = action,
    };
}

void noah_action_release(keypos_t key_pos, uint16_t action) {
    CHECK(release_call_count < ARRAY_SIZE(release_calls));
    release_calls[release_call_count++] = (action_call_t){
        .key_pos = key_pos,
        .action  = action,
    };
}

uint16_t timer_read(void) {
    timer_read_count++;
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

void clear_mods(void) {}
void clear_weak_mods(void) {}
void clear_oneshot_mods(void) {}
void clear_oneshot_locked_mods(void) {}
void send_keyboard_report(void) {}

void action_dispatch(uint16_t action) {
    CHECK(tap_call_count < ARRAY_SIZE(tap_calls));
    tap_calls[tap_call_count++] = (tap_call_t){
        .action = action,
        .row    = MATRIX_ROWS,
        .col    = MATRIX_COLS,
    };
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    (void)policy;
    CHECK(tap_call_count < ARRAY_SIZE(tap_calls));
    tap_calls[tap_call_count++] = (tap_call_t){
        .action = action,
        .row    = MATRIX_ROWS,
        .col    = MATRIX_COLS,
    };
}

void noah_emit_action_tap_at(keypos_t key_pos, uint16_t action, noah_emit_policy_t policy) {
    (void)policy;
    CHECK(tap_call_count < ARRAY_SIZE(tap_calls));
    tap_calls[tap_call_count++] = (tap_call_t){
        .action = action,
        .row    = key_pos.row,
        .col    = key_pos.col,
    };
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    CHECK(pointer_action_call_count < ARRAY_SIZE(pointer_action_calls));
    pointer_action_calls[pointer_action_call_count++] = (pointer_action_call_t){
        .action  = action,
        .pressed = pressed,
    };
}

void pointer_layer_policy_sync_layer_ownership_anchor(void) {}

void pointer_layer_policy_settle_record(void) {}

void pointer_layer_policy_take_back_qmk_toggle(uint16_t keycode, const keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    CHECK(mod_register_count < ARRAY_SIZE(mod_register_calls));
    mod_register_calls[mod_register_count++] = keycode;
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    CHECK(mod_unregister_count < ARRAY_SIZE(mod_unregister_calls));
    mod_unregister_calls[mod_unregister_count++] = keycode;
}

bool owned_keycode_is_supported(uint16_t keycode) {
    return keycode <= UINT8_MAX;
}

bool owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease) {
    CHECK(lease != NULL);
    if (owned_failed_acquire_keycode == (int16_t)keycode) {
        return false;
    }
    CHECK(owned_acquire_count < ARRAY_SIZE(owned_acquire_calls));
    owned_acquire_calls[owned_acquire_count++] = keycode;
    *lease                                     = (owned_keycode_lease_t){.active = true, .has_basic = true, .basic = (uint8_t)keycode};
    return true;
}

bool owned_keycode_acquire_parts(uint8_t mods, uint8_t basic, owned_keycode_lease_t *lease) {
    CHECK(lease != NULL && !lease->active);
    CHECK(owned_part_count < ARRAY_SIZE(owned_part_calls));
    *lease                             = (owned_keycode_lease_t){.active = true, .has_basic = basic != KC_NO, .basic = basic, .mods = mods};
    owned_part_calls[owned_part_count++] = *lease;
    return true;
}

bool owned_keycode_release(owned_keycode_lease_t *lease) {
    CHECK(lease != NULL);
    CHECK(lease->active);
    CHECK(owned_release_count < ARRAY_SIZE(owned_release_calls));
    owned_release_calls[owned_release_count++] = lease->basic;
    CHECK(owned_part_release_count < ARRAY_SIZE(owned_part_release_mods));
    owned_part_release_mods[owned_part_release_count++] = lease->mods;
    *lease                                     = (owned_keycode_lease_t){0};
    return true;
}

noah_custom_key_shortcut_t noah_custom_key_shortcut(uint8_t slot) {
    return slot < CUSTOM_KEY_SLOT_COUNT ? test_shortcuts[slot] : (noah_custom_key_shortcut_t){0};
}

static void test_shared_action_refcounts_press_and_release(void) {
    keypos_t key_a = test_keypos(0, 0);
    keypos_t key_b = test_keypos(0, 1);

    test_reset_stubs();

    held_action_register(key_a, TEST_SHARED_ACTION);
    held_action_register(key_b, TEST_SHARED_ACTION);

    CHECK(press_call_count == 1);
    CHECK(press_calls[0].action == TEST_SHARED_ACTION);
    CHECK(held_action_survives_flush(key_a, TEST_SHARED_ACTION));
    CHECK(held_action_survives_flush(key_b, TEST_SHARED_ACTION));

    held_action_unregister(key_a, TEST_SHARED_ACTION);
    CHECK(release_call_count == 0);
    CHECK(!held_action_survives_flush(key_a, TEST_SHARED_ACTION));
    CHECK(held_action_survives_flush(key_b, TEST_SHARED_ACTION));

    held_action_unregister(key_b, TEST_SHARED_ACTION);
    CHECK(release_call_count == 1);
    CHECK(release_calls[0].action == TEST_SHARED_ACTION);
    CHECK(!held_action_survives_flush(key_b, TEST_SHARED_ACTION));
}

static void test_per_key_action_dispatches_for_each_owner(void) {
    keypos_t key_a = test_keypos(1, 0);
    keypos_t key_b = test_keypos(1, 1);

    test_reset_stubs();

    held_action_register(key_a, TEST_PER_KEY_ACTION);
    held_action_register(key_b, TEST_PER_KEY_ACTION);

    CHECK(press_call_count == 2);
    CHECK(press_calls[0].action == TEST_PER_KEY_ACTION);
    CHECK(press_calls[1].action == TEST_PER_KEY_ACTION);

    held_action_unregister(key_a, TEST_PER_KEY_ACTION);
    CHECK(release_call_count == 1);
    CHECK(release_calls[0].action == TEST_PER_KEY_ACTION);

    held_action_unregister(key_b, TEST_PER_KEY_ACTION);
    CHECK(release_call_count == 2);
    CHECK(release_calls[1].action == TEST_PER_KEY_ACTION);
}

static void test_modifier_ownership_refcounts_without_action_dispatch(void) {
    keypos_t key_a = test_keypos(2, 0);
    keypos_t key_b = test_keypos(2, 1);

    test_reset_stubs();

    held_action_register(key_a, KC_LEFT_SHIFT);
    held_action_register(key_b, KC_LEFT_SHIFT);

    CHECK(mod_register_count == 1);
    CHECK(mod_register_calls[0] == KC_LEFT_SHIFT);
    CHECK(press_call_count == 0);
    CHECK(held_action_survives_flush(key_a, KC_LEFT_SHIFT));

    CHECK(held_modifier_release_owned_by_key(key_a));
    CHECK(mod_unregister_count == 0);

    held_action_unregister(key_b, KC_LEFT_SHIFT);
    CHECK(mod_unregister_count == 1);
    CHECK(mod_unregister_calls[0] == KC_LEFT_SHIFT);
    CHECK(release_call_count == 0);
    CHECK(held_action_survives_flush(key_b, KC_LEFT_SHIFT));
}

static void test_rebinding_same_key_releases_old_action_before_pressing_new(void) {
    keypos_t key_pos = test_keypos(3, 3);

    test_reset_stubs();

    held_action_register(key_pos, TEST_SHARED_ACTION);
    held_action_register(key_pos, TEST_SECOND_ACTION);

    CHECK(press_call_count == 2);
    CHECK(press_calls[0].action == TEST_SHARED_ACTION);
    CHECK(press_calls[1].action == TEST_SECOND_ACTION);
    CHECK(release_call_count == 1);
    CHECK(release_calls[0].action == TEST_SHARED_ACTION);

    held_action_unregister(key_pos, TEST_SECOND_ACTION);
    CHECK(release_call_count == 2);
    CHECK(release_calls[1].action == TEST_SECOND_ACTION);
}

static void test_literal_actions_keep_one_exact_lease_per_physical_owner(void) {
    keypos_t key_a = test_keypos(4, 0);
    keypos_t key_b = test_keypos(4, 1);

    test_reset_stubs();

    held_action_register(key_a, KC_C);
    held_action_register(key_b, KC_C);
    CHECK(owned_acquire_count == 2);
    CHECK(owned_acquire_calls[0] == KC_C);
    CHECK(owned_acquire_calls[1] == KC_C);
    CHECK(press_call_count == 0);

    held_action_unregister(key_b, KC_C);
    CHECK(owned_release_count == 1);
    CHECK(owned_release_calls[0] == KC_C);
    held_action_unregister(key_a, KC_C);
    CHECK(owned_release_count == 2);
    CHECK(owned_release_calls[1] == KC_C);
    CHECK(release_call_count == 0);
}

static void test_failed_literal_acquire_never_falls_back_to_unscoped_release(void) {
    keypos_t key_pos = test_keypos(4, 2);

    test_reset_stubs();
    owned_failed_acquire_keycode = KC_C;

    held_action_register(key_pos, KC_C);
    CHECK(owned_acquire_count == 0);
    CHECK(press_call_count == 0);
    CHECK(held_action_survives_flush(key_pos, KC_C));

    held_action_unregister(key_pos, KC_C);
    CHECK(owned_release_count == 0);
    CHECK(release_call_count == 0);
    CHECK(!held_action_survives_flush(key_pos, KC_C));
}

// A custom key holds its shortcut through one exact lease per owner (D-F26):
// Left Ctrl + Right Alt + K goes down as one set of parts and comes back up as
// the same parts, even when the profile changes the shortcut meanwhile.
static void test_custom_key_holds_its_shortcut_through_an_exact_lease(void) {
    keypos_t key_a = test_keypos(5, 0);
    keypos_t key_b = test_keypos(5, 1);

    test_reset_stubs();
    test_shortcuts[4] = (noah_custom_key_shortcut_t){.mods = MOD_BIT(KC_LEFT_CTRL) | MOD_BIT(KC_RIGHT_ALT), .key = KC_K};

    held_action_register(key_a, CUSTOM_KEY_4);
    CHECK(owned_part_count == 1);
    CHECK(owned_part_calls[0].mods == (MOD_BIT(KC_LEFT_CTRL) | MOD_BIT(KC_RIGHT_ALT)) && owned_part_calls[0].basic == KC_K && owned_part_calls[0].has_basic);
    CHECK(owned_acquire_count == 0 && press_call_count == 0);
    CHECK(held_action_survives_flush(key_a, CUSTOM_KEY_4));

    // A second owner takes its own lease; the profile changes the shortcut
    // while both are held.
    held_action_register(key_b, CUSTOM_KEY_4);
    CHECK(owned_part_count == 2);
    test_shortcuts[4] = (noah_custom_key_shortcut_t){.mods = MOD_BIT(KC_LEFT_GUI), .key = KC_J};

    held_action_unregister(key_b, CUSTOM_KEY_4);
    CHECK(owned_release_count == 1 && owned_release_calls[0] == KC_K && owned_part_release_mods[0] == (MOD_BIT(KC_LEFT_CTRL) | MOD_BIT(KC_RIGHT_ALT)));
    CHECK(held_action_release_owned_by_key(key_a));
    CHECK(owned_release_count == 2 && owned_release_calls[1] == KC_K && owned_part_release_mods[1] == (MOD_BIT(KC_LEFT_CTRL) | MOD_BIT(KC_RIGHT_ALT)));
    CHECK(release_call_count == 0 && press_call_count == 0);
    CHECK(!held_action_survives_flush(key_a, CUSTOM_KEY_4));
}

// Modifiers alone and a key alone are both shortcuts; a custom key without
// one holds nothing and never reaches generic dispatch.
static void test_custom_key_shortcut_parts_and_absence(void) {
    keypos_t key_pos = test_keypos(5, 2);

    test_reset_stubs();
    test_shortcuts[5] = (noah_custom_key_shortcut_t){.mods = MOD_BIT(KC_LEFT_CTRL) | MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_GUI)};
    test_shortcuts[6] = (noah_custom_key_shortcut_t){.key = MS_BTN1};

    held_action_register(key_pos, CUSTOM_KEY_5);
    CHECK(owned_part_count == 1 && owned_part_calls[0].mods == 0x0f && !owned_part_calls[0].has_basic);
    // Rebinding the same owner releases the old shortcut before the new one.
    held_action_register(key_pos, CUSTOM_KEY_6);
    CHECK(owned_release_count == 1 && owned_part_release_mods[0] == 0x0f);
    CHECK(owned_part_count == 2 && owned_part_calls[1].mods == 0u && owned_part_calls[1].basic == MS_BTN1);
    held_action_unregister(key_pos, CUSTOM_KEY_6);
    CHECK(owned_release_count == 2 && owned_release_calls[1] == MS_BTN1);

    held_action_register(key_pos, CUSTOM_KEY_7);
    CHECK(owned_part_count == 2 && owned_acquire_count == 0 && press_call_count == 0);
    CHECK(held_action_survives_flush(key_pos, CUSTOM_KEY_7));
    held_action_unregister(key_pos, CUSTOM_KEY_7);
    CHECK(owned_release_count == 2 && release_call_count == 0);
    CHECK(!held_action_survives_flush(key_pos, CUSTOM_KEY_7));
}

static void test_release_owned_by_key_reports_missing_bindings(void) {
    test_reset_stubs();

    CHECK(!held_action_release_owned_by_key(test_keypos(7, 7)));
    CHECK(!held_modifier_release_owned_by_key(test_keypos(7, 7)));
    CHECK(!held_repeat_release_owned_by_key(test_keypos(7, 7)));
}

static void test_repeat_tick_returns_without_timer_work_when_idle(void) {
    test_reset_stubs();

    held_repeat_tick();

    CHECK(timer_read_count == 0u);
    CHECK(tap_call_count == 0u);
}

static void test_repeat_binding_taps_immediately_and_on_tick_until_release(void) {
    keypos_t key_pos = test_keypos(5, 5);

    test_reset_stubs();

    held_repeat_start(key_pos, TEST_SHARED_ACTION, 25);

    CHECK(tap_call_count == 1);
    CHECK(tap_calls[0].action == TEST_SHARED_ACTION);
    CHECK(tap_calls[0].row == key_pos.row);
    CHECK(tap_calls[0].col == key_pos.col);
    CHECK(pointer_action_call_count == 1);
    CHECK(pointer_action_calls[0].action == TEST_SHARED_ACTION);
    CHECK(pointer_action_calls[0].pressed);

    fake_time = (uint16_t)(fake_time + 39);
    held_repeat_tick();
    CHECK(tap_call_count == 1);

    fake_time = (uint16_t)(fake_time + 1);
    held_repeat_tick();
    CHECK(tap_call_count == 2);
    CHECK(tap_calls[1].action == TEST_SHARED_ACTION);
    CHECK(tap_calls[1].row == key_pos.row);
    CHECK(tap_calls[1].col == key_pos.col);

    CHECK(held_repeat_release_owned_by_key(key_pos));
    CHECK(pointer_action_call_count == 2);
    CHECK(pointer_action_calls[1].action == TEST_SHARED_ACTION);
    CHECK(!pointer_action_calls[1].pressed);

    fake_time = (uint16_t)(fake_time + 80);
    held_repeat_tick();
    CHECK(tap_call_count == 2);
}

static void test_repeat_binding_preserves_rate_across_small_loop_drift(void) {
    keypos_t key_pos = test_keypos(5, 6);

    test_reset_stubs();

    held_repeat_start(key_pos, TEST_SHARED_ACTION, 100);
    CHECK(tap_call_count == 1);

    for (uint8_t i = 0; i < 10u; i++) {
        fake_time = (uint16_t)(fake_time + 11u);
        held_repeat_tick();
    }

    CHECK(tap_call_count == 12);
    for (uint8_t i = 1; i < tap_call_count; i++) {
        CHECK(tap_calls[i].action == TEST_SHARED_ACTION);
        CHECK(tap_calls[i].row == key_pos.row);
        CHECK(tap_calls[i].col == key_pos.col);
    }
}

static void test_repeat_binding_catches_up_after_scan_gap(void) {
    keypos_t key_pos = test_keypos(6, 6);

    test_reset_stubs();

    held_repeat_start(key_pos, TEST_SECOND_ACTION, 25);
    CHECK(tap_call_count == 1);

    fake_time = (uint16_t)(fake_time + 120);
    held_repeat_tick();

    CHECK(tap_call_count == 4);
    CHECK(tap_calls[1].action == TEST_SECOND_ACTION);
    CHECK(tap_calls[1].row == key_pos.row);
    CHECK(tap_calls[1].col == key_pos.col);
    CHECK(tap_calls[2].action == TEST_SECOND_ACTION);
    CHECK(tap_calls[2].row == key_pos.row);
    CHECK(tap_calls[2].col == key_pos.col);
    CHECK(tap_calls[3].action == TEST_SECOND_ACTION);
    CHECK(tap_calls[3].row == key_pos.row);
    CHECK(tap_calls[3].col == key_pos.col);

    fake_time = (uint16_t)(fake_time + 39);
    held_repeat_tick();
    CHECK(tap_call_count == 4);

    fake_time = (uint16_t)(fake_time + 1);
    held_repeat_tick();
    CHECK(tap_call_count == 5);
    CHECK(tap_calls[4].action == TEST_SECOND_ACTION);
    CHECK(tap_calls[4].row == key_pos.row);
    CHECK(tap_calls[4].col == key_pos.col);
}

static void test_repeat_binding_bounds_excessive_backlog_after_long_stall(void) {
    keypos_t key_pos = test_keypos(6, 7);

    test_reset_stubs();

    held_repeat_start(key_pos, TEST_SECOND_ACTION, 25);
    CHECK(tap_call_count == 1);

    fake_time = (uint16_t)(fake_time + 400);
    held_repeat_tick();

    CHECK(tap_call_count == 5);
    for (uint8_t i = 1; i < tap_call_count; i++) {
        CHECK(tap_calls[i].action == TEST_SECOND_ACTION);
        CHECK(tap_calls[i].row == key_pos.row);
        CHECK(tap_calls[i].col == key_pos.col);
    }

    fake_time = (uint16_t)(fake_time + 39);
    held_repeat_tick();
    CHECK(tap_call_count == 5);

    fake_time = (uint16_t)(fake_time + 1);
    held_repeat_tick();
    CHECK(tap_call_count == 6);
}

static void test_repeat_binding_rejects_rates_above_supported_range(void) {
    keypos_t key_pos = test_keypos(7, 0);

    test_reset_stubs();

    held_repeat_start(key_pos, TEST_SHARED_ACTION, (uint16_t)(KEY_BEHAVIOR_REPEAT_MAX_HZ + 1u));

    CHECK(tap_call_count == 0);
    CHECK(pointer_action_call_count == 0);
    CHECK(!held_repeat_release_owned_by_key(key_pos));
}

int main(void) {
    test_shared_action_refcounts_press_and_release();
    test_per_key_action_dispatches_for_each_owner();
    test_modifier_ownership_refcounts_without_action_dispatch();
    test_rebinding_same_key_releases_old_action_before_pressing_new();
    test_literal_actions_keep_one_exact_lease_per_physical_owner();
    test_failed_literal_acquire_never_falls_back_to_unscoped_release();
    test_custom_key_holds_its_shortcut_through_an_exact_lease();
    test_custom_key_shortcut_parts_and_absence();
    test_release_owned_by_key_reports_missing_bindings();
    test_repeat_tick_returns_without_timer_work_when_idle();
    test_repeat_binding_taps_immediately_and_on_tick_until_release();
    test_repeat_binding_preserves_rate_across_small_loop_drift();
    test_repeat_binding_catches_up_after_scan_gap();
    test_repeat_binding_bounds_excessive_backlog_after_long_stall();
    test_repeat_binding_rejects_rates_above_supported_range();

    puts("held_action host tests passed");
    return 0;
}
