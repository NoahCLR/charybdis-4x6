#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/lib/profile/runtime/profile_action_placement_v1.h"
#include "users/noah/lib/profile/runtime/profile_action_runtime_v1.h"

typedef struct {
    uint16_t keycode;
    bool     fallback_hold_active;
    uint8_t  real;
    uint8_t  weak;
    uint8_t  oneshot;
    uint8_t  oneshot_locked;
    uint8_t  row;
    uint8_t  col;
} tap_call_t;

static uint8_t fake_mods;
static uint8_t fake_weak_mods;
static uint8_t fake_oneshot_mods;
static uint8_t fake_oneshot_locked_mods;
static uint8_t send_keyboard_report_count;
static uint8_t fallback_hold_settle_real_mods;
static uint8_t fallback_hold_settle_weak_mods;
static uint8_t fallback_hold_settle_oneshot_mods;
static uint8_t fallback_hold_settle_oneshot_locked_mods;

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
    fake_mods                                = 0;
    fake_weak_mods                           = 0;
    fake_oneshot_mods                        = 0;
    fake_oneshot_locked_mods                 = 0;
    send_keyboard_report_count               = 0;
    fallback_hold_settle_real_mods           = 0;
    fallback_hold_settle_weak_mods           = 0;
    fallback_hold_settle_oneshot_mods        = 0;
    fallback_hold_settle_oneshot_locked_mods = 0;
    fallback_hold_activation_count           = 0;
    fallback_hold_active                     = false;
    action_tap_call                          = (tap_call_t){0};
    synthetic_qmk_tap_call                   = (tap_call_t){0};
    literal_tap_call                         = (tap_call_t){0};
    action_tap_call_count                    = 0;
    synthetic_qmk_tap_call_count             = 0;
    literal_tap_call_count                   = 0;

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
        .row                  = MATRIX_ROWS,
        .col                  = MATRIX_COLS,
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
    fake_mods |= fallback_hold_settle_real_mods;
    fake_weak_mods |= fallback_hold_settle_weak_mods;
    fake_oneshot_mods |= fallback_hold_settle_oneshot_mods;
    fake_oneshot_locked_mods |= fallback_hold_settle_oneshot_locked_mods;
    return true;
}

bool layer_ownership_is_locked(uint8_t layer) {
    return layer < LAYER_COUNT && layer_locked_state[layer];
}

bool layer_ownership_toggle_lock_state(uint8_t layer) {
    (void)layer;
    return true;
}

bool layer_ownership_goto(uint8_t layer) {
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

bool pd_mode_toggle_lock_state_at(pd_mode_mask_t mode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_toggle_lock_state(mode);
}

void noah_action_tap(uint16_t action) {
    action_tap_call = test_current_tap_call(action);
    action_tap_call_count++;
}

void noah_action_tap_at(keypos_t key_pos, uint16_t action) {
    action_tap_call     = test_current_tap_call(action);
    action_tap_call.row = key_pos.row;
    action_tap_call.col = key_pos.col;
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

// The ledger is faked here, so a literal tap is recorded as QMK's tap.
void owned_keycode_tap_literal(uint16_t keycode) {
    tap_code16(keycode);
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

void pointer_layer_policy_sync_layer_lock_anchor(void) {}

void pointer_layer_policy_settle_record(void) {}

void pointer_layer_policy_take_back_qmk_toggle(uint16_t keycode, const keyrecord_t *record) {
    (void)keycode;
    (void)record;
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

#define NOAH_PD_MODE_TEST_ROW(name, mode_keycode) [PD_MODE_INDEX_##name] = {.mode_flag = PD_MODE_##name, .keycode = (mode_keycode), .lock_action = mode_keycode##_LOCK},
const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {NOAH_PD_MODE_LIST(NOAH_PD_MODE_TEST_ROW)};
#undef NOAH_PD_MODE_TEST_ROW

bool is_pd_mode_lock_action(uint16_t action) {
    return action == PD_SLOT_4_LOCK;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    return keycode == PD_SLOT_4 ? PD_MODE_ARROW : 0;
}

static void test_action_descriptor_classifies_common_actions(void) {
    noah_action_desc_t layer_lock   = noah_action_describe(LOCK_LAYER(2));
    noah_action_desc_t momentary    = noah_action_describe(MO(3));
    noah_action_desc_t layer_tap    = noah_action_describe(LT(4, KC_V));
    noah_action_desc_t raw_layer    = noah_action_describe(DF(5));
    noah_action_desc_t layer_toggle = noah_action_describe(TG(2));
    noah_action_desc_t layer_jump   = noah_action_describe(TO(5));
    noah_action_desc_t qmk_behavior = noah_action_describe(OSM(MOD_LSFT));
    noah_action_desc_t pd_key       = noah_action_describe(PD_SLOT_4);
    noah_action_desc_t pd_lock      = noah_action_describe(PD_SLOT_4_LOCK);
    noah_action_desc_t macro_action = noah_action_describe(VIA_MACRO_0);
    noah_action_desc_t custom       = noah_action_describe(CUSTOM_KEY_0 + 1);
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
    CHECK(noah_action_desc_supported_as_combo_output(momentary));
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
    CHECK(!noah_action_desc_supported_as_combo_output(layer_tap));
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

    CHECK(raw_layer.kind == NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION);
    CHECK(!noah_action_desc_supported_as_combo_output(raw_layer));
    CHECK(!noah_action_desc_is_owned_momentary_layer(raw_layer));
    CHECK(!noah_action_desc_is_layer_tap(raw_layer));
    CHECK(!noah_action_desc_supported_as_behavior_keycode(raw_layer));
    CHECK(!noah_action_desc_supported_as_authored_action(raw_layer, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(!noah_action_desc_supported_as_authored_action(raw_layer, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(!noah_action_desc_supported_as_authored_action(raw_layer, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(!noah_action_desc_uses_authored_layer_tap_contract(raw_layer));
    CHECK(noah_action_desc_default_tap_uses_action_keycode(raw_layer));
    CHECK(!noah_action_desc_is_pure_modifier_literal(raw_layer));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(raw_layer));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(raw_layer));
    CHECK(noah_action_desc_supports_fallback_hold(raw_layer));
    CHECK(!noah_action_desc_source_layer_uses_desc_layer(raw_layer));
    CHECK(noah_action_desc_source_layer(raw_layer) == UINT8_MAX);
    CHECK(noah_action_desc_default_tap_action(raw_layer) == DF(5));

    // TG() is the lock LOCK_LAYER() toggles; TO() is its own owned kind with
    // the same authored reach.
    CHECK(layer_toggle.kind == NOAH_ACTION_KIND_LAYER_LOCK);
    CHECK(layer_toggle.layer == 2);
    CHECK(layer_toggle.action == TG(2));
    CHECK(noah_action_desc_supported_as_combo_output(layer_toggle));
    CHECK(noah_action_desc_consumes_direct_press(layer_toggle));
    CHECK(noah_action_desc_releases_momentary_layer_before_action(layer_toggle));
    CHECK(noah_action_desc_default_tap_action(layer_toggle) == KC_NO);
    CHECK(noah_action_describe(TG(LAYER_COUNT)).kind == NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION);

    // TT() holds its layer exactly as MO() does; its toggle tap is a built-in
    // behaviour, not a different action.
    noah_action_desc_t tap_toggle = noah_action_describe(TT(3));
    CHECK(tap_toggle.kind == NOAH_ACTION_KIND_LAYER_HOLD);
    CHECK(tap_toggle.layer == 3);
    CHECK(noah_action_desc_is_momentary_layer_keycode(tap_toggle));
    CHECK(noah_action_desc_is_runtime_handled_keycode(tap_toggle));
    CHECK(noah_action_desc_source_layer(tap_toggle) == 3);
    CHECK(noah_action_describe(TT(LAYER_COUNT)).kind == NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION);

    // OSL() holds its layer like MO() and taps itself: its default tap is the
    // action that arms the one-shot. A combo cannot carry it.
    noah_action_desc_t oneshot = noah_action_describe(OSL(4));
    CHECK(oneshot.kind == NOAH_ACTION_KIND_LAYER_ONESHOT);
    CHECK(noah_action_desc_is_layer_oneshot(oneshot));
    CHECK(oneshot.layer == 4);
    CHECK(noah_action_desc_is_momentary_layer_keycode(oneshot));
    CHECK(noah_action_desc_is_runtime_handled_keycode(oneshot));
    CHECK(noah_action_desc_requires_per_key_hold(oneshot));
    CHECK(noah_action_desc_source_layer(oneshot) == 4);
    CHECK(noah_action_desc_default_tap_action(oneshot) == OSL(4));
    CHECK(noah_action_desc_supported_as_behavior_keycode(oneshot));
    CHECK(noah_action_desc_supported_as_authored_action(oneshot, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(!noah_action_desc_supported_as_authored_action(oneshot, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(noah_action_desc_supported_as_combo_output(oneshot));
    CHECK(noah_action_describe(OSL(LAYER_COUNT)).kind == NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION);

    // LM() holds its layer like MO(); its modifiers are the key's built-in hold.
    noah_action_desc_t layer_mod = noah_action_describe(LM(3, MOD_LSFT | MOD_LGUI));
    CHECK(layer_mod.kind == NOAH_ACTION_KIND_LAYER_MOD);
    CHECK(noah_action_desc_is_layer_mod(layer_mod));
    CHECK(layer_mod.layer == 3);
    CHECK(noah_action_desc_is_momentary_layer_keycode(layer_mod));
    CHECK(noah_action_desc_is_runtime_handled_keycode(layer_mod));
    CHECK(noah_action_keycode_layer_mod_mods(LM(3, MOD_LSFT | MOD_LGUI)) == LSG(KC_NO));
    // A right-hand modifier keeps its right-hand bit (0x10): 0x14 is right Alt.
    CHECK(noah_action_keycode_layer_mod_mods(LM(3, 0x14u)) == 0x1400u);
    CHECK(noah_action_describe(LM(LAYER_COUNT, MOD_LSFT)).kind == NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION);

    CHECK(layer_jump.kind == NOAH_ACTION_KIND_LAYER_GOTO);
    CHECK(noah_action_desc_is_layer_goto(layer_jump));
    CHECK(layer_jump.layer == 5);
    CHECK(noah_action_desc_supported_as_combo_output(layer_jump));
    CHECK(noah_action_desc_has_capability(layer_jump, NOAH_ACTION_CAP_LAYER_AFFECTING));
    CHECK(noah_action_desc_is_press_only(layer_jump));
    CHECK(noah_action_desc_is_layer_action(layer_jump));
    CHECK(noah_action_desc_supported_as_behavior_keycode(layer_jump));
    CHECK(noah_action_desc_supported_as_authored_action(layer_jump, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(noah_action_desc_supported_as_authored_action(layer_jump, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(noah_action_desc_supported_as_authored_action(layer_jump, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(noah_action_desc_consumes_direct_press(layer_jump));
    CHECK(noah_action_desc_releases_momentary_layer_before_action(layer_jump));
    CHECK(!noah_action_desc_supports_fallback_hold(layer_jump));
    CHECK(noah_action_desc_default_tap_action(layer_jump) == KC_NO);
    CHECK(noah_action_describe(TO(LAYER_COUNT)).kind == NOAH_ACTION_KIND_UNSUPPORTED_LAYER_ACTION);

    CHECK(qmk_behavior.kind == NOAH_ACTION_KIND_QMK_BEHAVIOR);
    CHECK(noah_action_desc_supported_as_combo_output(qmk_behavior));
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
    CHECK(noah_action_desc_consumes_direct_press(macro_action));
    CHECK(!noah_action_desc_uses_held_lifecycle_for_press_and_hold(macro_action));
    CHECK(!noah_action_desc_default_tap_uses_action_keycode(macro_action));
    CHECK(!noah_action_desc_is_pure_modifier_literal(macro_action));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(macro_action));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(macro_action));
    CHECK(!noah_action_desc_supports_fallback_hold(macro_action));
    CHECK(noah_action_desc_default_tap_action(macro_action) == KC_NO);

    // A custom key is a row's own keycode and a combo output, never a step.
    CHECK(custom.kind == NOAH_ACTION_KIND_CUSTOM_KEY);
    CHECK(noah_action_desc_supported_as_behavior_keycode(custom));
    CHECK(noah_action_desc_supported_as_combo_output(custom));
    CHECK(!noah_action_desc_supported_as_authored_action(custom, NOAH_ACTION_AUTHORED_USE_TAP));
    CHECK(!noah_action_desc_supported_as_authored_action(custom, NOAH_ACTION_AUTHORED_USE_HOLD_PRESS_AND_HOLD));
    CHECK(!noah_action_desc_supported_as_authored_action(custom, NOAH_ACTION_AUTHORED_USE_HOLD_OTHER));
    CHECK(noah_action_describe(CUSTOM_KEY_63).kind == NOAH_ACTION_KIND_CUSTOM_KEY && noah_action_describe(PD_SLOT_0).kind != NOAH_ACTION_KIND_CUSTOM_KEY);
    CHECK(noah_action_desc_uses_held_lifecycle_for_press_and_hold(custom));
    CHECK(!noah_action_desc_default_tap_uses_action_keycode(custom));
    CHECK(!noah_action_desc_is_pure_modifier_literal(custom));
    CHECK(!noah_action_desc_source_sets_momentary_layer_flag(custom));
    CHECK(!noah_action_desc_source_sets_layer_tap_flag(custom));
    CHECK(!noah_action_desc_supports_fallback_hold(custom));
    CHECK(noah_action_desc_default_tap_action(custom) == KC_NO);

    CHECK(literal.kind == NOAH_ACTION_KIND_LITERAL);
    CHECK(!noah_action_desc_is_layer_lock(literal));
    CHECK(noah_action_desc_supported_as_combo_output(literal));
    CHECK(!noah_action_desc_is_macro(literal));
    CHECK(!noah_action_desc_is_custom_key(literal));
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
    CHECK(action_tap_call.fallback_hold_active);
    CHECK(action_tap_call_count == 1);
    CHECK(action_tap_call.keycode == KC_C);
}

static void test_explicit_action_emit_can_skip_fallback_hold_settlement(void) {
    test_reset_stubs();

    noah_emit_action_tap(KC_RIGHT, NOAH_EMIT_POLICY_NONE);

    CHECK(fallback_hold_activation_count == 0);
    CHECK(action_tap_call_count == 1);
    CHECK(action_tap_call.keycode == KC_RIGHT);
    CHECK(!action_tap_call.fallback_hold_active);
}

static void test_explicit_action_emit_at_preserves_origin_key_pos(void) {
    keypos_t key_pos = {.row = 2, .col = 3};

    test_reset_stubs();

    noah_emit_action_tap_at(key_pos, KC_LEFT, NOAH_EMIT_POLICY_NONE);

    CHECK(action_tap_call_count == 1);
    CHECK(action_tap_call.keycode == KC_LEFT);
    CHECK(action_tap_call.row == key_pos.row);
    CHECK(action_tap_call.col == key_pos.col);
}

static void test_synthetic_qmk_emit_settles_fallback_hold_without_touching_mod_state(void) {
    test_reset_stubs();
    fake_mods                = MOD_BIT(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    noah_emit_synthetic_qmk_tap(KC_RIGHT, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(synthetic_qmk_tap_call.fallback_hold_active);
    CHECK(synthetic_qmk_tap_call_count == 1);
    CHECK(synthetic_qmk_tap_call.keycode == KC_RIGHT);
    CHECK(synthetic_qmk_tap_call.real == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(synthetic_qmk_tap_call.weak == MOD_BIT(KC_RIGHT_ALT));
    CHECK(synthetic_qmk_tap_call.oneshot == MOD_BIT(KC_LEFT_GUI));
    CHECK(synthetic_qmk_tap_call.oneshot_locked == MOD_BIT(KC_RIGHT_GUI));
    CHECK(send_keyboard_report_count == 0);
}

static void test_masked_synthetic_qmk_emit_settles_fallback_hold_and_restores_post_settlement_mod_state(void) {
    test_reset_stubs();
    fake_mods                                = MOD_BIT(KC_LEFT_SHIFT);
    fake_weak_mods                           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods                        = MOD_BIT(KC_LEFT_GUI);
    fallback_hold_settle_real_mods           = MOD_BIT(KC_LEFT_ALT);
    fallback_hold_settle_oneshot_locked_mods = MOD_BIT(KC_RIGHT_ALT);

    noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(KC_DOWN, (MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_RIGHT_ALT)), true);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(synthetic_qmk_tap_call.fallback_hold_active);
    CHECK(fake_mods == (MOD_BIT(KC_LEFT_SHIFT) | MOD_BIT(KC_LEFT_ALT)));
    CHECK(fake_weak_mods == MOD_BIT(KC_RIGHT_ALT));
    CHECK(fake_oneshot_mods == MOD_BIT(KC_LEFT_GUI));
    CHECK(fake_oneshot_locked_mods == MOD_BIT(KC_RIGHT_ALT));
    CHECK(synthetic_qmk_tap_call_count == 1);
    CHECK(synthetic_qmk_tap_call.keycode == KC_DOWN);
    CHECK(synthetic_qmk_tap_call.real == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(synthetic_qmk_tap_call.oneshot == MOD_BIT(KC_LEFT_GUI));
    CHECK(send_keyboard_report_count == 2);
}

static void test_literal_emit_can_suspend_and_restore_mods(void) {
    test_reset_stubs();
    fake_mods                = MOD_BIT(KC_LEFT_ALT) | MOD_BIT(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    noah_emit_literal_tap(G(KC_C), NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS);

    CHECK(fallback_hold_activation_count == 1);
    CHECK(literal_tap_call.fallback_hold_active);
    CHECK(literal_tap_call_count == 1);
    CHECK(literal_tap_call.keycode == G(KC_C));
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

// Where an action may be placed: the one rule set compile-time validation and
// the keyboard's check of a saved profile share.
static void test_action_placement_rules(void) {
    const uint16_t everywhere[] = {KC_A, G(KC_C), LOCK_LAYER(2), TG(2), TO(0)};
    for (uint8_t i = 0; i < ARRAY_SIZE(everywhere); i++) {
        CHECK(noah_action_supported_at(everywhere[i], NOAH_ACTION_PLACEMENT_KEY));
        CHECK(noah_action_supported_at(everywhere[i], NOAH_ACTION_PLACEMENT_BEHAVIOR_TAP));
        CHECK(noah_action_supported_at(everywhere[i], NOAH_ACTION_PLACEMENT_COMBO_OUTPUT));
    }

    // Layer holds need a held key: a key, a press-and-hold branch or a combo,
    // never a tap or another hold mode.
    CHECK(noah_action_supported_at(MO(2), NOAH_ACTION_PLACEMENT_KEY));
    CHECK(noah_action_supported_at(MO(2), NOAH_ACTION_PLACEMENT_BEHAVIOR_HOLD_PRESS_AND_HOLD));
    CHECK(!noah_action_supported_at(MO(2), NOAH_ACTION_PLACEMENT_BEHAVIOR_HOLD_OTHER));
    CHECK(!noah_action_supported_at(MO(2), NOAH_ACTION_PLACEMENT_BEHAVIOR_TAP));
    // A combo holds its output, so layer holds and one-shots work there; LT()
    // keeps its own tap/hold decision and is a key only.
    CHECK(noah_action_supported_at(MO(2), NOAH_ACTION_PLACEMENT_COMBO_OUTPUT));
    CHECK(noah_action_supported_at(TT(2), NOAH_ACTION_PLACEMENT_COMBO_OUTPUT));
    CHECK(!noah_action_supported_at(LT(2, KC_A), NOAH_ACTION_PLACEMENT_COMBO_OUTPUT));
    CHECK(noah_action_supported_at(OSL(2), NOAH_ACTION_PLACEMENT_BEHAVIOR_TAP));
    CHECK(noah_action_supported_at(OSL(2), NOAH_ACTION_PLACEMENT_COMBO_OUTPUT));

    // Layer actions the runtime does not own are refused wherever they go.
    // LM() holds a layer and its modifiers: a key or a combo, not a behaviour step.
    CHECK(noah_action_supported_at(LM(1, MOD_LSFT), NOAH_ACTION_PLACEMENT_KEY));
    CHECK(noah_action_supported_at(LM(1, MOD_LSFT), NOAH_ACTION_PLACEMENT_COMBO_OUTPUT));
    CHECK(!noah_action_supported_at(LM(1, MOD_LSFT), NOAH_ACTION_PLACEMENT_BEHAVIOR_TAP));
    CHECK(!noah_action_supported_at(LM(1, MOD_LSFT), NOAH_ACTION_PLACEMENT_BEHAVIOR_HOLD_PRESS_AND_HOLD));

    const uint16_t unowned[] = {DF(1), PDF(1), LM(LAYER_COUNT, MOD_LSFT), TG(LAYER_COUNT), OSL(LAYER_COUNT)};
    for (uint8_t i = 0; i < ARRAY_SIZE(unowned); i++) {
        CHECK(!noah_action_supported_at(unowned[i], NOAH_ACTION_PLACEMENT_KEY));
        CHECK(!noah_action_supported_at(unowned[i], NOAH_ACTION_PLACEMENT_BEHAVIOR_TAP));
        CHECK(!noah_action_supported_at(unowned[i], NOAH_ACTION_PLACEMENT_BEHAVIOR_HOLD_PRESS_AND_HOLD));
        CHECK(!noah_action_supported_at(unowned[i], NOAH_ACTION_PLACEMENT_COMBO_OUTPUT));
    }
}

// A userspace code no block assigns does nothing and goes nowhere: not a
// behaviour's key, a step or a combo output. The blocks' last codes keep
// their kinds.
static void test_unassigned_userspace_codes_are_refused_everywhere(void) {
    const uint16_t unassigned[] = {NOAH_KEYCODE_PD_HOLD_BASE + PD_MODE_COUNT, NOAH_KEYCODE_PD_LOCK_BASE + NOAH_KEYCODE_PD_RESERVED - 1, LAYER_LOCK_BASE + LAYER_COUNT, NOAH_KEYCODE_USERSPACE_END, QK_USER_MAX};
    const noah_action_placement_t placements[] = {NOAH_ACTION_PLACEMENT_KEY, NOAH_ACTION_PLACEMENT_BEHAVIOR_TAP, NOAH_ACTION_PLACEMENT_BEHAVIOR_HOLD_PRESS_AND_HOLD, NOAH_ACTION_PLACEMENT_BEHAVIOR_HOLD_OTHER, NOAH_ACTION_PLACEMENT_COMBO_OUTPUT};
    for (uint8_t i = 0; i < ARRAY_SIZE(unassigned); i++) {
        CHECK(noah_action_describe(unassigned[i]).kind == NOAH_ACTION_KIND_UNASSIGNED_USER);
        for (uint8_t p = 0; p < ARRAY_SIZE(placements); p++)
            CHECK(!noah_action_supported_at(unassigned[i], placements[p]));
    }
    CHECK(noah_action_describe(CUSTOM_KEY_63).kind == NOAH_ACTION_KIND_CUSTOM_KEY);
    CHECK(noah_action_describe(PD_SLOT_4).kind == NOAH_ACTION_KIND_PD_MODE_HOLD);
    CHECK(noah_action_describe(PD_SLOT_4_LOCK).kind == NOAH_ACTION_KIND_PD_MODE_LOCK);
    CHECK(noah_action_describe(LOCK_LAYER(LAYER_COUNT - 1)).kind == NOAH_ACTION_KIND_LAYER_LOCK);
    CHECK(noah_action_describe(QK_USER - 1).kind != NOAH_ACTION_KIND_UNASSIGNED_USER);

    const noah_profile_action_v1_t stray = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = NOAH_KEYCODE_USERSPACE_END};
    CHECK(!noah_profile_action_placement_v1_supported(&stray, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP));
    CHECK(!noah_profile_action_placement_v1_supported(&stray, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT));
}

// The keyboard's check of a saved profile asks the same rules of a Profile
// Wire action, placement for placement.
static void test_saved_profile_placement_matches_the_rules(void) {
    const noah_profile_action_v1_t key_a       = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = KC_A};
    const noah_profile_action_v1_t hold        = {.kind = NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY, .operand = 2};
    const noah_profile_action_v1_t lock        = {.kind = NOAH_PROFILE_ACTION_V1_LAYER_LOCK, .operand = 2};
    const noah_profile_action_v1_t default_lay = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = DF(1)};
    const noah_profile_action_v1_t oneshot     = {.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .operand = OSL(2)};
    const noah_profile_action_v1_t invalid     = {.kind = NOAH_PROFILE_ACTION_V1_LAYER_LOCK, .operand = LAYER_COUNT};

    CHECK(noah_profile_action_placement_v1_supported(&key_a, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT));
    CHECK(noah_profile_action_placement_v1_supported(&hold, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_KEY));
    CHECK(noah_profile_action_placement_v1_supported(&hold, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_HOLD_PRESS_AND_HOLD));
    CHECK(!noah_profile_action_placement_v1_supported(&hold, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_HOLD_OTHER));
    CHECK(!noah_profile_action_placement_v1_supported(&hold, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP));
    CHECK(noah_profile_action_placement_v1_supported(&hold, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT));
    CHECK(noah_profile_action_placement_v1_supported(&lock, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP));
    CHECK(!noah_profile_action_placement_v1_supported(&default_lay, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP));
    CHECK(noah_profile_action_placement_v1_supported(&oneshot, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP));
    CHECK(noah_profile_action_placement_v1_supported(&oneshot, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT));
    CHECK(!noah_profile_action_placement_v1_supported(&invalid, NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP));
}

int main(void) {
    test_action_placement_rules();
    test_saved_profile_placement_matches_the_rules();
    test_unassigned_userspace_codes_are_refused_everywhere();
    test_action_descriptor_classifies_common_actions();
    test_action_dispatch_keeps_runtime_default_policy();
    test_explicit_action_emit_can_skip_fallback_hold_settlement();
    test_explicit_action_emit_at_preserves_origin_key_pos();
    test_synthetic_qmk_emit_settles_fallback_hold_without_touching_mod_state();
    test_masked_synthetic_qmk_emit_settles_fallback_hold_and_restores_post_settlement_mod_state();
    test_literal_emit_can_suspend_and_restore_mods();

    puts("action_dispatch host tests passed");
    return 0;
}
