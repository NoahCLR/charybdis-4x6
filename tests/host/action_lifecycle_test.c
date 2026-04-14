#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/noah_keymap_ids.h"

enum {
    TEST_QMK_BEHAVIOR_ACTION = OSM(MOD_LSFT),
    TEST_CUSTOM_ACTION       = NOAH_KEYMAP_SAFE_RANGE + 1,
    TEST_RAW_LAYER_ACTION    = LT(2, KC_C),
};

typedef struct {
    uint16_t keycode;
    bool     pressed;
    uint8_t  layer;
    keypos_t key_pos;
} test_call_t;

static test_call_t layer_toggle_call;
static test_call_t layer_press_call;
static test_call_t layer_release_call;
static test_call_t synthetic_tap_call;
static test_call_t synthetic_qmk_tap_call;
static test_call_t synthetic_record_call;
static test_call_t synthetic_qmk_record_call;
static test_call_t tap_code16_call;
static test_call_t register_code16_call;
static test_call_t unregister_code16_call;
static test_call_t owned_register_call;
static test_call_t owned_unregister_call;
static test_call_t pointer_action_call_1;
static test_call_t pointer_action_call_2;
static uint8_t     pointer_action_call_count;

static uint8_t macro_dispatch_calls;
static uint8_t pd_toggle_calls;
static uint8_t pd_press_calls;
static uint8_t pd_release_calls;
static uint8_t split_sync_calls;

static bool macro_dispatch_result;
static bool pd_toggle_result;
static bool pd_press_result;
static bool pd_release_result;
static bool owned_register_result;
static bool owned_unregister_result;

static const pd_mode_def_t test_pd_mode_def = {
    .mode_flag   = PD_MODE_ARROW,
    .keycode     = ARROW_MODE,
    .lock_action = ARROW_MODE_LOCK,
};

void split_runtime_sync(void);

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
    layer_toggle_call         = (test_call_t){0};
    layer_press_call          = (test_call_t){0};
    layer_release_call        = (test_call_t){0};
    synthetic_tap_call        = (test_call_t){0};
    synthetic_qmk_tap_call    = (test_call_t){0};
    synthetic_record_call     = (test_call_t){0};
    synthetic_qmk_record_call = (test_call_t){0};
    tap_code16_call           = (test_call_t){0};
    register_code16_call      = (test_call_t){0};
    unregister_code16_call    = (test_call_t){0};
    owned_register_call       = (test_call_t){0};
    owned_unregister_call     = (test_call_t){0};
    pointer_action_call_1     = (test_call_t){0};
    pointer_action_call_2     = (test_call_t){0};
    pointer_action_call_count = 0;
    macro_dispatch_calls      = 0;
    pd_toggle_calls           = 0;
    pd_press_calls            = 0;
    pd_release_calls          = 0;
    split_sync_calls          = 0;
    macro_dispatch_result     = false;
    pd_toggle_result          = false;
    pd_press_result           = false;
    pd_release_result         = false;
    owned_register_result     = false;
    owned_unregister_result   = false;
}

bool macro_dispatch(uint16_t action) {
    macro_dispatch_calls++;
    return action == MACRO_0 && macro_dispatch_result;
}

bool is_pd_mode_lock_action(uint16_t action) {
    return action == ARROW_MODE_LOCK;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    return keycode == ARROW_MODE ? PD_MODE_ARROW : 0;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    return action == ARROW_MODE_LOCK ? &test_pd_mode_def : NULL;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    CHECK(mode == PD_MODE_ARROW);
    pd_toggle_calls++;
    if (pd_toggle_result) {
        split_runtime_sync();
    }
    return pd_toggle_result;
}

bool pd_mode_handle_keycode_press(uint16_t keycode) {
    if (keycode == ARROW_MODE) {
        pd_press_calls++;
        return pd_press_result;
    }
    return false;
}

bool pd_mode_handle_keycode_release(uint16_t keycode) {
    if (keycode == ARROW_MODE) {
        pd_release_calls++;
        return pd_release_result;
    }
    return false;
}

bool layer_ownership_toggle_lock_state(uint8_t layer) {
    layer_toggle_call.layer = layer;
    return true;
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    layer_press_call.key_pos = key_pos;
    layer_press_call.layer   = layer;
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    layer_release_call.key_pos = key_pos;
    return true;
}

void split_runtime_sync(void) {
    split_sync_calls++;
}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    synthetic_tap_call.keycode = keycode;
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    synthetic_qmk_tap_call.keycode = keycode;
}

void noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    synthetic_record_call.keycode = keycode;
    synthetic_record_call.pressed = pressed;
}

void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count) {
    (void)tap_count;
    synthetic_qmk_record_call.keycode = keycode;
    synthetic_qmk_record_call.pressed = pressed;
}

bool owned_keycode_register(uint16_t keycode) {
    owned_register_call.keycode = keycode;
    return owned_register_result;
}

bool owned_keycode_unregister(uint16_t keycode) {
    owned_unregister_call.keycode = keycode;
    return owned_unregister_result;
}

void tap_code16(uint16_t keycode) {
    tap_code16_call.keycode = keycode;
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    test_call_t *call = pointer_action_call_count == 0 ? &pointer_action_call_1 : &pointer_action_call_2;
    call->keycode     = action;
    call->pressed     = pressed;
    pointer_action_call_count++;
}

void register_code16(uint16_t keycode) {
    register_code16_call.keycode = keycode;
}

void unregister_code16(uint16_t keycode) {
    unregister_code16_call.keycode = keycode;
}

static void test_descriptor_classifies_dispatch_shapes(void) {
    test_reset_stubs();

    CHECK(noah_action_desc_is_press_only(noah_action_describe(LOCK_LAYER(3))));
    CHECK(noah_action_desc_is_press_only(noah_action_describe(ARROW_MODE_LOCK)));
    CHECK(noah_action_desc_is_press_only(noah_action_describe(MACRO_0)));
    CHECK(noah_action_desc_requires_per_key_hold(noah_action_describe(MO(2))));
    CHECK(noah_action_desc_uses_shared_hold(noah_action_describe(KC_C)));
}

static void test_tap_handles_layer_lock_and_pd_lock(void) {
    test_reset_stubs();

    noah_action_tap(LOCK_LAYER(4));
    CHECK(macro_dispatch_calls == 0);
    CHECK(layer_toggle_call.layer == 4);
    CHECK(split_sync_calls == 0);
    CHECK(tap_code16_call.keycode == KC_NO);

    test_reset_stubs();
    pd_toggle_result = true;

    noah_action_tap(ARROW_MODE_LOCK);
    CHECK(macro_dispatch_calls == 0);
    CHECK(pd_toggle_calls == 1);
    CHECK(split_sync_calls == 1);
    CHECK(tap_code16_call.keycode == KC_NO);

    test_reset_stubs();
    pd_toggle_result = false;

    noah_action_tap(ARROW_MODE_LOCK);
    CHECK(macro_dispatch_calls == 0);
    CHECK(pd_toggle_calls == 1);
    CHECK(split_sync_calls == 0);
}

static void test_tap_routes_macro_custom_qmk_and_plain_actions(void) {
    test_reset_stubs();
    macro_dispatch_result = true;

    noah_action_tap(MACRO_0);
    CHECK(macro_dispatch_calls == 1);
    CHECK(tap_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_tap(TEST_CUSTOM_ACTION);
    CHECK(macro_dispatch_calls == 1);
    CHECK(synthetic_tap_call.keycode == TEST_CUSTOM_ACTION);
    CHECK(tap_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_tap(TEST_QMK_BEHAVIOR_ACTION);
    CHECK(macro_dispatch_calls == 1);
    CHECK(synthetic_qmk_tap_call.keycode == TEST_QMK_BEHAVIOR_ACTION);
    CHECK(tap_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_tap(KC_C);
    CHECK(macro_dispatch_calls == 1);
    CHECK(tap_code16_call.keycode == KC_C);

    test_reset_stubs();

    noah_action_tap(MS_BTN1);
    CHECK(macro_dispatch_calls == 1);
    CHECK(tap_code16_call.keycode == MS_BTN1);
    CHECK(pointer_action_call_count == 2);
    CHECK(pointer_action_call_1.keycode == MS_BTN1);
    CHECK(pointer_action_call_1.pressed);
    CHECK(pointer_action_call_2.keycode == MS_BTN1);
    CHECK(!pointer_action_call_2.pressed);
}

static void test_tap_ignores_raw_layer_actions(void) {
    test_reset_stubs();

    noah_action_tap(TEST_RAW_LAYER_ACTION);
    CHECK(macro_dispatch_calls == 1);
    CHECK(tap_code16_call.keycode == KC_NO);
    CHECK(synthetic_tap_call.keycode == KC_NO);
    CHECK(synthetic_qmk_tap_call.keycode == KC_NO);
}

static void test_press_routes_pd_mode_momentary_qmk_custom_and_plain(void) {
    keypos_t key_pos = test_keypos(1, 2);

    test_reset_stubs();
    pd_press_result = true;

    noah_action_press(key_pos, ARROW_MODE);
    CHECK(macro_dispatch_calls == 1);
    CHECK(pd_press_calls == 1);
    CHECK(layer_press_call.layer == 0);
    CHECK(register_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_press(key_pos, MO(5));
    CHECK(macro_dispatch_calls == 1);
    CHECK(pd_press_calls == 0);
    CHECK(layer_press_call.layer == 5);
    CHECK(layer_press_call.key_pos.row == key_pos.row);
    CHECK(layer_press_call.key_pos.col == key_pos.col);
    CHECK(register_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_press(key_pos, TEST_QMK_BEHAVIOR_ACTION);
    CHECK(synthetic_qmk_record_call.keycode == TEST_QMK_BEHAVIOR_ACTION);
    CHECK(synthetic_qmk_record_call.pressed);
    CHECK(register_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_press(key_pos, TEST_CUSTOM_ACTION);
    CHECK(synthetic_record_call.keycode == TEST_CUSTOM_ACTION);
    CHECK(synthetic_record_call.pressed);
    CHECK(register_code16_call.keycode == KC_NO);

    test_reset_stubs();
    owned_register_result = true;

    noah_action_press(key_pos, KC_RIGHT_ALT);
    CHECK(owned_register_call.keycode == KC_RIGHT_ALT);
    CHECK(register_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_press(key_pos, KC_RIGHT_ALT);
    CHECK(owned_register_call.keycode == KC_RIGHT_ALT);
    CHECK(register_code16_call.keycode == KC_RIGHT_ALT);
}

static void test_press_ignores_raw_layer_actions_and_one_shot_actions(void) {
    keypos_t key_pos = test_keypos(2, 3);

    test_reset_stubs();

    noah_action_press(key_pos, TEST_RAW_LAYER_ACTION);
    CHECK(register_code16_call.keycode == KC_NO);
    CHECK(layer_press_call.layer == 0);
    CHECK(synthetic_record_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_press(key_pos, LOCK_LAYER(1));
    CHECK(macro_dispatch_calls == 0);
    CHECK(layer_toggle_call.layer == 1);
    CHECK(pd_press_calls == 0);
    CHECK(register_code16_call.keycode == KC_NO);
}

static void test_release_routes_press_only_pd_mode_momentary_qmk_custom_and_plain(void) {
    keypos_t key_pos = test_keypos(4, 5);

    test_reset_stubs();

    noah_action_release(key_pos, LOCK_LAYER(2));
    CHECK(pd_release_calls == 0);
    CHECK(layer_release_call.key_pos.row == 0);
    CHECK(unregister_code16_call.keycode == KC_NO);

    test_reset_stubs();
    pd_release_result = true;

    noah_action_release(key_pos, ARROW_MODE);
    CHECK(pd_release_calls == 1);
    CHECK(layer_release_call.key_pos.row == 0);
    CHECK(unregister_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_release(key_pos, MO(6));
    CHECK(pd_release_calls == 0);
    CHECK(layer_release_call.key_pos.row == key_pos.row);
    CHECK(layer_release_call.key_pos.col == key_pos.col);
    CHECK(unregister_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_release(key_pos, TEST_QMK_BEHAVIOR_ACTION);
    CHECK(synthetic_qmk_record_call.keycode == TEST_QMK_BEHAVIOR_ACTION);
    CHECK(!synthetic_qmk_record_call.pressed);
    CHECK(unregister_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_release(key_pos, TEST_CUSTOM_ACTION);
    CHECK(synthetic_record_call.keycode == TEST_CUSTOM_ACTION);
    CHECK(!synthetic_record_call.pressed);
    CHECK(unregister_code16_call.keycode == KC_NO);

    test_reset_stubs();
    owned_unregister_result = true;

    noah_action_release(key_pos, KC_RIGHT_ALT);
    CHECK(owned_unregister_call.keycode == KC_RIGHT_ALT);
    CHECK(unregister_code16_call.keycode == KC_NO);

    test_reset_stubs();

    noah_action_release(key_pos, KC_RIGHT_ALT);
    CHECK(owned_unregister_call.keycode == KC_RIGHT_ALT);
    CHECK(unregister_code16_call.keycode == KC_RIGHT_ALT);
}

static void test_release_ignores_raw_layer_actions(void) {
    keypos_t key_pos = test_keypos(3, 1);

    test_reset_stubs();

    noah_action_release(key_pos, TEST_RAW_LAYER_ACTION);
    CHECK(unregister_code16_call.keycode == KC_NO);
    CHECK(layer_release_call.key_pos.row == 0);
    CHECK(synthetic_record_call.keycode == KC_NO);
}

int main(void) {
    test_descriptor_classifies_dispatch_shapes();
    test_tap_handles_layer_lock_and_pd_lock();
    test_tap_routes_macro_custom_qmk_and_plain_actions();
    test_tap_ignores_raw_layer_actions();
    test_press_routes_pd_mode_momentary_qmk_custom_and_plain();
    test_press_ignores_raw_layer_actions_and_one_shot_actions();
    test_release_routes_press_only_pd_mode_momentary_qmk_custom_and_plain();
    test_release_ignores_raw_layer_actions();

    puts("action_lifecycle host tests passed");
    return 0;
}
