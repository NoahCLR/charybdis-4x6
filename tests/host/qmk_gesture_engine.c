// Compile QMK's real combo and tapping engines with their real record types.
// This boundary marshals scalar events into the firmware host harness; it does
// not copy either state machine or assume the host stub has QMK's ABI.
#include "action.h"
#include "action_layer.h"
#include "action_tapping.h"
#include "action_util.h"
#include "process_combo.h"
#include "debug.h"
#include "keymap_introspection.h"

extern void     gesture_deliver(uint16_t code, uint8_t row, uint8_t col, bool down, uint16_t time, uint8_t taps, bool combo, uint8_t context);
extern uint16_t gesture_keycode(uint8_t row, uint8_t col);

static uint8_t sixteen_rules;
static const uint16_t click_spam[]     = {0x7e85, MS_BTN3, COMBO_END};
static const uint16_t alpha_three[]    = {KC_M, KC_COMM, KC_DOT, COMBO_END};
static const uint16_t alpha_four[]     = {KC_M, KC_COMM, KC_DOT, LT(3, KC_SLSH), COMBO_END};
static const uint16_t mouse_two[]      = {MS_BTN1, MS_BTN2, COMBO_END};
static const uint16_t mouse_three[]    = {MS_BTN1, MS_BTN2, 0x7e80, COMBO_END};
static const uint16_t mouse_four[]     = {MS_BTN1, MS_BTN2, 0x7e80, LT(3, KC_SLSH), COMBO_END};
static const uint16_t copy_paste[]     = {G(KC_C), G(KC_V), COMBO_END};
static const uint16_t alpha_cmd[]      = {KC_N, KC_M, COMBO_END};
static const uint16_t mouse_cmd[]      = {MS_BTN1, 0x7e81, COMBO_END};
static const uint16_t tab[]            = {KC_D, LT(3, KC_F), COMBO_END};
// Sixteen members on rows 6 and 7 (EXTRA_LONG_COMBOS), and the fifteen-key
// chord inside it.
static const uint16_t sixteen[]        = {KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12, KC_F13, KC_F14, KC_F15, KC_F16, COMBO_END};
static const uint16_t fifteen[]        = {KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12, KC_F13, KC_F14, KC_F15, COMBO_END};
static combo_t        gesture_combos[] = {
    {.keys = click_spam, .keycode = 0x7e42}, {.keys = alpha_three, .keycode = G(KC_T)}, {.keys = alpha_four, .keycode = G(KC_N)}, {.keys = mouse_two, .keycode = MS_BTN6}, {.keys = mouse_three, .keycode = G(KC_T)}, {.keys = mouse_four, .keycode = G(KC_N)}, {.keys = copy_paste, .keycode = G(KC_A)}, {.keys = alpha_cmd, .keycode = KC_LGUI}, {.keys = mouse_cmd, .keycode = KC_LGUI}, {.keys = tab, .keycode = KC_TAB}, {.keys = sixteen, .keycode = KC_ENTER}, {.keys = fifteen, .keycode = KC_ESCAPE},
};
_Static_assert(MAX_COMBO_LENGTH == 16 && COMBO_KEY_BUFFER_LENGTH == 16, "the engine is built with sixteen-member combos");
uint16_t get_combo_term(uint16_t i, combo_t *c) {
    return i == 2 || i == 5 ? 100 : 50;
}

uint16_t combo_count(void) {
    return sizeof(gesture_combos) / sizeof(gesture_combos[0]);
}
combo_t *combo_get(uint16_t i) {
    return &gesture_combos[i];
}
uint16_t engine_get_record_keycode(keyrecord_t *r, bool update) {
    return r->keycode ? r->keycode : gesture_keycode(r->event.key.row, r->event.key.col);
}
uint16_t engine_get_event_keycode(keyevent_t e, bool update) {
    return gesture_keycode(e.key.row, e.key.col);
}
uint16_t engine_keymap_key_to_keycode(uint8_t layer, keypos_t pos) {
    return gesture_keycode(pos.row, pos.col);
}
extern bool gesture_is_tap_record(uint16_t code, uint8_t *row, uint8_t *col, bool pressed, uint16_t time, uint8_t taps, uint8_t type, uint8_t *context, bool native, bool fallback);
bool is_tap_record(keyrecord_t *r) {
    uint16_t code = engine_get_record_keycode(r, false);
    // BK's is_tap_record: the record hook has the last word, given QMK's own
    // answer and the keycode hook's.
    bool native = IS_QK_LAYER_TAP(code) || IS_QK_MOD_TAP(code) || IS_QK_LAYER_TAP_TOGGLE(code) || IS_QK_ONE_SHOT_LAYER(code) || IS_QK_ONE_SHOT_MOD(code);
    return gesture_is_tap_record(code, &r->event.key.row, &r->event.key.col, r->event.pressed, r->event.time, r->tap.count, r->event.type, &r->user_data, native, is_tap_keycode_user(code, native));
}
void engine_process_record(keyrecord_t *r) {
    if (IS_EVENT(r->event)) gesture_deliver(engine_get_record_keycode(r, false), r->event.key.row, r->event.key.col, r->event.pressed, r->event.time, r->tap.count, r->event.type == COMBO_EVENT, r->user_data);
}
void    debug_record(keyrecord_t r) {}
void    clear_keyboard(void) {}
void    engine_del_weak_mods(uint8_t mods) {}
void    engine_clear_weak_mods(void) {}
void    engine_wait_ms(uint16_t ms) {}
uint8_t engine_get_mods(void) {
    return 0;
}
void           engine_set_mods(uint8_t mods) {}
debug_config_t debug_config;

void gesture_engine_event(uint8_t row, uint8_t col, bool down, uint16_t time, uint8_t context) {
    keyrecord_t r = {.user_data = context, .event = {.key = {.row = row, .col = col}, .pressed = down, .time = time, .type = KEY_EVENT}};
    if (process_combo(gesture_keycode(row, col), &r)) action_tapping_process(r);
}
void gesture_engine_scan(uint16_t time) {
    keyrecord_t tick = {.event = {.type = TICK_EVENT, .time = time}};
    action_tapping_process(tick);
    combo_task();
}
uint8_t biton16(uint16_t bits) {
    return bits ? (uint8_t)(31 - __builtin_clz((unsigned)bits)) : 0;
}
void     debug_event(keyevent_t e) {}
void     process_record_tap_hint(keyrecord_t *r) {}
// Enough of QMK's keycode-to-action mapping for tapping's release rules: a
// modifier or layer-tap released during another key's tapping term is held
// back until that term resolves, as on the keyboard.
action_t layer_switch_get_action(keypos_t key) {
    uint16_t code = gesture_keycode(key.row, key.col);
    if (IS_QK_LAYER_TAP(code)) return (action_t){.code = ACTION_LAYER_TAP_KEY(QK_LAYER_TAP_GET_LAYER(code), QK_LAYER_TAP_GET_TAP_KEYCODE(code))};
    if (IS_MODIFIER_KEYCODE(code)) return (action_t){.code = ACTION_KEY(code)};
    return (action_t){.code = 0};
}

// QMK asks the firmware's participation hook through scalars: the firmware
// side reads the host stub's record layout, not this one.
extern bool gesture_combo_should_trigger(uint16_t index, uint8_t row, uint8_t col, bool pressed, bool key_event, uint8_t context);
bool        engine_combo_key_record_allowed(uint16_t index, combo_t *combo, uint16_t keycode, keyrecord_t *record) {
    (void)combo;
    (void)keycode;
    if (index == 11 && sixteen_rules) return false;
    return gesture_combo_should_trigger(index, record->event.key.row, record->event.key.col, record->event.pressed, record->event.type == KEY_EVENT, record->user_data);
}

// Scalar/read-only bridge: the host stub's combo_t has a different layout.
const uint16_t *gesture_engine_combo_keys(uint16_t index) { return gesture_combos[index].keys; }
uint16_t gesture_engine_combo_output(uint16_t index) { return gesture_combos[index].keycode; }
bool gesture_engine_combo_active(uint16_t index) { return gesture_combos[index].active; }
bool gesture_engine_combo_disabled(uint16_t index) { return gesture_combos[index].disabled; }

void gesture_engine_combo_output_set(uint16_t index, uint16_t code) { gesture_combos[index].keycode = code; }

// Exercise production's three matching hooks on the full 16-bit chord.
void gesture_engine_sixteen_rules(uint8_t rules) { sixteen_rules = rules; }
bool get_combo_must_hold(uint16_t index, combo_t *combo) { return index == 10 && (sixteen_rules & 1u); }
bool get_combo_must_tap(uint16_t index, combo_t *combo) { return index == 10 && (sixteen_rules & 2u); }
bool get_combo_must_press_in_order(uint16_t index, combo_t *combo) { return index == 10 && (sixteen_rules & 4u); }
uint16_t gesture_engine_combo_state(uint16_t index) { return gesture_combos[index].state; }
