#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/synthetic_record.h"

typedef struct {
    uint16_t keycode;
    bool     fallback_hold_active;
    uint8_t  real;
    uint8_t  weak;
    uint8_t  oneshot;
    uint8_t  oneshot_locked;
} tap_call_t;

static uint8_t fake_mods;
static uint8_t fake_weak_mods;
static uint8_t fake_oneshot_mods;
static uint8_t fake_oneshot_locked_mods;
static uint8_t send_keyboard_report_count;

static uint8_t fallback_hold_activation_count;
static bool    fallback_hold_active;

static bool layer_locked_state[LAYER_COUNT];

static tap_call_t action_tap_call;
static tap_call_t synthetic_qmk_tap_call;
static tap_call_t literal_tap_call;

static uint8_t action_tap_call_count;
static uint8_t synthetic_qmk_tap_call_count;
static uint8_t literal_tap_call_count;

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

static void test_reset_stubs(void) {
    fake_mods                      = 0;
    fake_weak_mods                 = 0;
    fake_oneshot_mods              = 0;
    fake_oneshot_locked_mods       = 0;
    send_keyboard_report_count     = 0;
    fallback_hold_activation_count = 0;
    fallback_hold_active           = false;
    action_tap_call                = (tap_call_t){0};
    synthetic_qmk_tap_call         = (tap_call_t){0};
    literal_tap_call               = (tap_call_t){0};
    action_tap_call_count          = 0;
    synthetic_qmk_tap_call_count   = 0;
    literal_tap_call_count         = 0;

    for (uint8_t layer = 0; layer < LAYER_COUNT; layer++) {
        layer_locked_state[layer] = false;
    }
}

static tap_call_t test_current_tap_call(uint16_t keycode) {
    return (tap_call_t){
        .keycode              = keycode,
        .fallback_hold_active = fallback_hold_active,
        .real                 = fake_mods,
        .weak                 = fake_weak_mods,
        .oneshot              = fake_oneshot_mods,
        .oneshot_locked       = fake_oneshot_locked_mods,
    };
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

void send_keyboard_report(void) {
    send_keyboard_report_count++;
}

bool noah_key_runtime_settle_pending_fallback_hold(void) {
    fallback_hold_activation_count++;
    fallback_hold_active = true;
    return true;
}

bool layer_ownership_is_locked(uint8_t layer) {
    return layer < LAYER_COUNT && layer_locked_state[layer];
}

bool layer_ownership_toggle_lock_state(uint8_t layer) {
    (void)layer;
    return true;
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    (void)key_pos;
    (void)layer;
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    (void)key_pos;
    return true;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    (void)action;
    return NULL;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

void noah_action_tap(uint16_t action) {
    action_tap_call = test_current_tap_call(action);
    action_tap_call_count++;
}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    (void)keycode;
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    synthetic_qmk_tap_call = test_current_tap_call(keycode);
    synthetic_qmk_tap_call_count++;
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

void register_code16(uint16_t keycode) {
    (void)keycode;
}

void unregister_code16(uint16_t keycode) {
    (void)keycode;
}

void tap_code16(uint16_t keycode) {
    literal_tap_call = test_current_tap_call(keycode);
    literal_tap_call_count++;
}

bool is_pd_mode_lock_action(uint16_t action) {
    return action == ARROW_MODE_LOCK;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    return keycode == ARROW_MODE ? PD_MODE_ARROW : 0;
}

static void test_action_descriptor_classifies_common_actions(void) {
    noah_action_desc_t layer_lock   = noah_action_describe(LOCK_LAYER(2));
    noah_action_desc_t momentary    = noah_action_describe(MO(3));
    noah_action_desc_t layer_tap    = noah_action_describe(LT(4, KC_V));
    noah_action_desc_t layer_jump   = noah_action_describe(TO(5));
    noah_action_desc_t qmk_behavior = noah_action_describe(OSM(MOD_LSFT));
    noah_action_desc_t pd_key       = noah_action_describe(ARROW_MODE);
    noah_action_desc_t pd_lock      = noah_action_describe(ARROW_MODE_LOCK);
    noah_action_desc_t macro_action = noah_action_describe(MACRO_0);
    noah_action_desc_t custom       = noah_action_describe(NOAH_KEYMAP_SAFE_RANGE + 1);
    noah_action_desc_t literal      = noah_action_describe(KC_C);

    CHECK(layer_lock.kind == NOAH_ACTION_KIND_LAYER_LOCK);
    CHECK(layer_lock.layer == 2);
    CHECK(noah_action_desc_has_capability(layer_lock, NOAH_ACTION_CAP_LAYER_AFFECTING));
    CHECK(noah_action_desc_is_press_only(layer_lock));
    CHECK(noah_action_desc_is_layer_action(layer_lock));
    CHECK(noah_action_desc_supported_as_behavior_keycode(layer_lock));
    CHECK(noah_action_desc_supported_as_authored_action(layer_lock, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(noah_action_desc_supported_as_authored_action(layer_lock, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(noah_action_desc_supported_as_authored_action(layer_lock, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(noah_action_desc_consumes_direct_press(layer_lock));
    CHECK(noah_action_desc_releases_momentary_layer_before_action(layer_lock));
    CHECK(!noah_action_desc_uses_held_lifecycle_for_press_and_hold(layer_lock));
    CHECK(!noah_action_desc_is_runtime_handled_keycode(layer_lock));
    CHECK(!noah_action_desc_default_tap_uses_action_keycode(layer_lock));
    CHECK(!noah_action_desc_is_pure_modifier_literal(layer_lock));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(layer_lock));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(layer_lock));
    CHECK(!noah_action_desc_supports_fallback_hold(layer_lock));
    CHECK(!noah_action_desc_source_layer_uses_desc_layer(layer_lock));
    CHECK(noah_action_desc_source_layer(layer_lock) == UINT8_MAX);
    CHECK(noah_action_desc_default_tap_action(layer_lock) == KC_NO);

    CHECK(momentary.kind == NOAH_ACTION_KIND_LAYER_HOLD);
    CHECK(noah_action_desc_is_raw_qmk_layer_action(momentary));
    CHECK(momentary.layer == 3);
    CHECK(noah_action_desc_requires_per_key_hold(momentary));
    CHECK(noah_action_desc_supported_as_behavior_keycode(momentary));
    CHECK(!noah_action_desc_supported_as_authored_action(momentary, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(noah_action_desc_supported_as_authored_action(momentary, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(!noah_action_desc_supported_as_authored_action(momentary, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(!noah_action_desc_consumes_direct_press(momentary));
    CHECK(noah_action_desc_is_runtime_handled_keycode(momentary));
    CHECK(noah_action_desc_is_momentary_layer_keycode(momentary));
    CHECK(noah_action_desc_uses_held_lifecycle_for_press_and_hold(momentary));
    CHECK(noah_action_desc_source_sets_momentary_layer_flag(momentary));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(momentary));
    CHECK(noah_action_desc_source_layer_uses_desc_layer(momentary));
    CHECK(noah_action_desc_source_layer(momentary) == 3);
    CHECK(!noah_action_desc_default_tap_uses_action_keycode(momentary));
    CHECK(!noah_action_desc_is_pure_modifier_literal(momentary));
    CHECK(!noah_action_desc_supports_fallback_hold(momentary));
    CHECK(noah_action_desc_default_tap_action(momentary) == KC_NO);

    CHECK(layer_tap.kind == NOAH_ACTION_KIND_LAYER_TAP);
    CHECK(noah_action_desc_is_raw_qmk_layer_action(layer_tap));
    CHECK(layer_tap.layer == 4);
    CHECK(!noah_action_desc_is_owned_momentary_layer(layer_tap));
    CHECK(noah_action_desc_supported_as_behavior_keycode(layer_tap));
    CHECK(!noah_action_desc_supported_as_authored_action(layer_tap, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(!noah_action_desc_supported_as_authored_action(layer_tap, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(!noah_action_desc_supported_as_authored_action(layer_tap, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(noah_action_desc_uses_authored_layer_tap_contract(layer_tap));
    CHECK(noah_action_desc_default_tap_uses_layer_tap_keycode(layer_tap));
    CHECK(noah_action_desc_source_sets_momentary_layer_flag(layer_tap));
    CHECK(noah_action_desc_source_sets_layer_tap_flag(layer_tap));
    CHECK(!noah_action_desc_is_runtime_handled_keycode(layer_tap));
    CHECK(!noah_action_desc_default_tap_uses_action_keycode(layer_tap));
    CHECK(!noah_action_desc_is_pure_modifier_literal(layer_tap));
    CHECK(!noah_action_desc_supports_fallback_hold(layer_tap));
    CHECK(noah_action_desc_source_layer_uses_desc_layer(layer_tap));
    CHECK(noah_action_desc_source_layer(layer_tap) == 4);
    CHECK(noah_action_desc_default_tap_action(layer_tap) == KC_V);

    CHECK(layer_jump.kind == NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION);
    CHECK(noah_action_desc_is_raw_qmk_layer_action(layer_jump));
    CHECK(!noah_action_desc_is_owned_momentary_layer(layer_jump));
    CHECK(!noah_action_desc_is_layer_tap(layer_jump));
    CHECK(!noah_action_desc_supported_as_behavior_keycode(layer_jump));
    CHECK(!noah_action_desc_supported_as_authored_action(layer_jump, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(!noah_action_desc_supported_as_authored_action(layer_jump, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(!noah_action_desc_supported_as_authored_action(layer_jump, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(!noah_action_desc_uses_authored_layer_tap_contract(layer_jump));
    CHECK(noah_action_desc_default_tap_uses_action_keycode(layer_jump));
    CHECK(!noah_action_desc_is_pure_modifier_literal(layer_jump));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(layer_jump));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(layer_jump));
    CHECK(noah_action_desc_supports_fallback_hold(layer_jump));
    CHECK(!noah_action_desc_source_layer_uses_desc_layer(layer_jump));
    CHECK(noah_action_desc_source_layer(layer_jump) == UINT8_MAX);
    CHECK(noah_action_desc_default_tap_action(layer_jump) == TO(5));

    CHECK(qmk_behavior.kind == NOAH_ACTION_KIND_QMK_BEHAVIOR);
    CHECK(!noah_action_desc_is_raw_qmk_layer_action(qmk_behavior));
    CHECK(noah_action_desc_supported_as_behavior_keycode(qmk_behavior));
    CHECK(noah_action_desc_supported_as_authored_action(qmk_behavior, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(noah_action_desc_supported_as_authored_action(qmk_behavior, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(noah_action_desc_supported_as_authored_action(qmk_behavior, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(noah_action_desc_uses_held_lifecycle_for_press_and_hold(qmk_behavior));
    CHECK(noah_action_desc_default_tap_uses_action_keycode(qmk_behavior));
    CHECK(!noah_action_desc_is_pure_modifier_literal(qmk_behavior));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(qmk_behavior));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(qmk_behavior));
    CHECK(!noah_action_desc_supports_fallback_hold(qmk_behavior));
    CHECK(noah_action_desc_default_tap_action(qmk_behavior) == OSM(MOD_LSFT));

    CHECK(pd_key.pd_mode == PD_MODE_ARROW);
    CHECK(pd_key.kind == NOAH_ACTION_KIND_PD_MODE_HOLD);
    CHECK(noah_action_desc_is_pd_mode_action(pd_key));
    CHECK(noah_action_desc_supported_as_behavior_keycode(pd_key));
    CHECK(noah_action_desc_supported_as_authored_action(pd_key, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(noah_action_desc_supported_as_authored_action(pd_key, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(noah_action_desc_supported_as_authored_action(pd_key, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(!noah_action_desc_consumes_direct_press(pd_key));
    CHECK(noah_action_desc_is_runtime_handled_keycode(pd_key));
    CHECK(noah_action_desc_uses_held_lifecycle_for_press_and_hold(pd_key));
    CHECK(!noah_action_desc_default_tap_uses_action_keycode(pd_key));
    CHECK(!noah_action_desc_is_pure_modifier_literal(pd_key));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(pd_key));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(pd_key));
    CHECK(!noah_action_desc_supports_fallback_hold(pd_key));
    CHECK(noah_action_desc_default_tap_action(pd_key) == KC_NO);

    CHECK(pd_lock.kind == NOAH_ACTION_KIND_PD_MODE_LOCK);
    CHECK(noah_action_desc_is_press_only(pd_lock));
    CHECK(noah_action_desc_has_capability(pd_lock, NOAH_ACTION_CAP_PD_MODE_AFFECTING));
    CHECK(noah_action_desc_consumes_direct_press(pd_lock));
    CHECK(!noah_action_desc_is_runtime_handled_keycode(pd_lock));
    CHECK(!noah_action_desc_uses_held_lifecycle_for_press_and_hold(pd_lock));
    CHECK(!noah_action_desc_default_tap_uses_action_keycode(pd_lock));
    CHECK(!noah_action_desc_is_pure_modifier_literal(pd_lock));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(pd_lock));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(pd_lock));
    CHECK(!noah_action_desc_supports_fallback_hold(pd_lock));
    CHECK(noah_action_desc_default_tap_action(pd_lock) == KC_NO);

    CHECK(macro_action.kind == NOAH_ACTION_KIND_MACRO);
    CHECK(noah_action_desc_is_press_only(macro_action));
    CHECK(noah_action_desc_supported_as_behavior_keycode(macro_action));
    CHECK(noah_action_desc_supported_as_authored_action(macro_action, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(noah_action_desc_supported_as_authored_action(macro_action, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(noah_action_desc_supported_as_authored_action(macro_action, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(!noah_action_desc_consumes_direct_press(macro_action));
    CHECK(!noah_action_desc_uses_held_lifecycle_for_press_and_hold(macro_action));
    CHECK(!noah_action_desc_default_tap_uses_action_keycode(macro_action));
    CHECK(!noah_action_desc_is_pure_modifier_literal(macro_action));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(macro_action));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(macro_action));
    CHECK(!noah_action_desc_supports_fallback_hold(macro_action));
    CHECK(noah_action_desc_default_tap_action(macro_action) == KC_NO);

    CHECK(custom.kind == NOAH_ACTION_KIND_KEYMAP_CUSTOM);
    CHECK(noah_action_desc_supported_as_behavior_keycode(custom));
    CHECK(noah_action_desc_supported_as_authored_action(custom, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(noah_action_desc_supported_as_authored_action(custom, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(noah_action_desc_supported_as_authored_action(custom, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(noah_action_desc_uses_held_lifecycle_for_press_and_hold(custom));
    CHECK(!noah_action_desc_default_tap_uses_action_keycode(custom));
    CHECK(!noah_action_desc_is_pure_modifier_literal(custom));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(custom));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(custom));
    CHECK(!noah_action_desc_supports_fallback_hold(custom));
    CHECK(noah_action_desc_default_tap_action(custom) == KC_NO);

    CHECK(literal.kind == NOAH_ACTION_KIND_LITERAL);
    CHECK(!noah_action_desc_is_layer_lock(literal));
    CHECK(!noah_action_desc_is_raw_qmk_layer_action(literal));
    CHECK(!noah_action_desc_is_macro(literal));
    CHECK(!noah_action_desc_is_keymap_custom(literal));
    CHECK(noah_action_desc_supported_as_behavior_keycode(literal));
    CHECK(noah_action_desc_supported_as_authored_action(literal, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(noah_action_desc_supported_as_authored_action(literal, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(noah_action_desc_supported_as_authored_action(literal, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(!noah_action_desc_consumes_direct_press(literal));
    CHECK(noah_action_desc_uses_held_lifecycle_for_press_and_hold(literal));
    CHECK(!noah_action_desc_is_runtime_handled_keycode(literal));
    CHECK(noah_action_desc_default_tap_uses_action_keycode(literal));
    CHECK(!noah_action_desc_is_pure_modifier_literal(literal));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(literal));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(literal));
    CHECK(noah_action_desc_supports_fallback_hold(literal));
    CHECK(!noah_action_desc_source_layer_uses_desc_layer(literal));
    CHECK(noah_action_desc_source_layer(literal) == UINT8_MAX);
    CHECK(noah_action_desc_default_tap_action(literal) == KC_C);

    literal = noah_action_describe(KC_LEFT_SHIFT);
    CHECK(noah_action_desc_is_pure_modifier_literal(literal));
    CHECK(noah_action_desc_default_tap_action(literal) == KC_LEFT_SHIFT);
}

static void test_action_dispatch_keeps_runtime_default_policy(void) {
    test_reset_stubs();

    action_dispatch(KC_C);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(action_tap_call_count == 1);
    CHECK(action_tap_call.keycode == KC_C);
    CHECK(action_tap_call.fallback_hold_active);
}

static void test_explicit_action_emit_can_skip_fallback_hold_settlement(void) {
    test_reset_stubs();

    noah_emit_action_tap(KC_RIGHT, NOAH_EMIT_POLICY_NONE);

    CHECK(fallback_hold_activation_count == 0);
    CHECK(action_tap_call_count == 1);
    CHECK(action_tap_call.keycode == KC_RIGHT);
    CHECK(!action_tap_call.fallback_hold_active);
}

static void test_synthetic_qmk_emit_settles_fallback_hold_without_touching_mod_state(void) {
    test_reset_stubs();
    fake_mods                = MOD_BIT(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    noah_emit_synthetic_qmk_tap(KC_RIGHT, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(synthetic_qmk_tap_call_count == 1);
    CHECK(synthetic_qmk_tap_call.keycode == KC_RIGHT);
    CHECK(synthetic_qmk_tap_call.fallback_hold_active);
    CHECK(synthetic_qmk_tap_call.real == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(synthetic_qmk_tap_call.weak == MOD_BIT(KC_RIGHT_ALT));
    CHECK(synthetic_qmk_tap_call.oneshot == MOD_BIT(KC_LEFT_GUI));
    CHECK(synthetic_qmk_tap_call.oneshot_locked == MOD_BIT(KC_RIGHT_GUI));
    CHECK(send_keyboard_report_count == 0);
}

static void test_literal_emit_can_suspend_and_restore_mods(void) {
    test_reset_stubs();
    fake_mods                = MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    noah_emit_literal_tap(G(KC_C), NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(literal_tap_call_count == 1);
    CHECK(literal_tap_call.keycode == G(KC_C));
    CHECK(literal_tap_call.fallback_hold_active);
    CHECK(literal_tap_call.real == 0);
    CHECK(literal_tap_call.weak == 0);
    CHECK(literal_tap_call.oneshot == 0);
    CHECK(literal_tap_call.oneshot_locked == 0);
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT)));
    CHECK(fake_weak_mods == MOD_BIT(KC_RIGHT_ALT));
    CHECK(fake_oneshot_mods == MOD_BIT(KC_LEFT_GUI));
    CHECK(fake_oneshot_locked_mods == MOD_BIT(KC_RIGHT_GUI));
    CHECK(send_keyboard_report_count == 2);
}

int main(void) {
    test_action_descriptor_classifies_common_actions();
    test_action_dispatch_keeps_runtime_default_policy();
    test_explicit_action_emit_can_skip_fallback_hold_settlement();
    test_synthetic_qmk_emit_settles_fallback_hold_without_touching_mod_state();
    test_literal_emit_can_suspend_and_restore_mods();

    puts("action_dispatch host tests passed");
    return 0;
}
