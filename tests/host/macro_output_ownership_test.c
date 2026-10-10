#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "qmk_stub.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/macro/macro_payload.h"

static uint32_t now;
static uint8_t  raw_keys[32], visible_keys[32], physical_mods, modifier_owners[8], visible_mods;
static unsigned edges[256];
bool            keyboard_report_mods_override_user(uint8_t *mods);
uint32_t        macro_payload_host_setting(void) {
    return 0u;
}
uint8_t macro_payload_host_os(void) {
    return 0u;
}
uint32_t timer_read32(void) {
    return now;
}
uint32_t timer_elapsed32(uint32_t before) {
    return now - before;
}
void wait_ms(uint16_t delay) {
    (void)delay;
    assert(false);
}
void pointer_layer_policy_note_action(uint16_t key, bool pressed) {
    (void)key;
    (void)pressed;
}
bool keyboard_mod_ownership_can_register_mods(uint8_t mods) {
    for (uint8_t i = 0; i < 8; i++)
        if ((mods & (1u << i)) && modifier_owners[i] == 255u) return false;
    return true;
}
bool keyboard_mod_ownership_can_unregister_mods(uint8_t mods) {
    for (uint8_t i = 0; i < 8; i++)
        if ((mods & (1u << i)) && modifier_owners[i] == 0u) return false;
    return true;
}
bool keyboard_mod_ownership_register_mods(uint8_t mods) {
    bool changed = false;
    for (uint8_t i = 0; i < 8; i++)
        if (mods & (1u << i)) {
            changed |= modifier_owners[i] == 0u;
            modifier_owners[i]++;
        }
    send_keyboard_report();
    return changed;
}
void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    for (uint8_t i = 0; i < 8; i++)
        if (mods & (1u << i)) {
            assert(modifier_owners[i]);
            modifier_owners[i]--;
        }
    send_keyboard_report();
}
void register_code(uint8_t key) {
    raw_keys[key / 8u] |= (uint8_t)(1u << (key % 8u));
    send_keyboard_report();
}
void unregister_code(uint8_t key) {
    raw_keys[key / 8u] &= (uint8_t)~(1u << (key % 8u));
    send_keyboard_report();
}
void tap_code16(uint16_t key) {
    (void)key;
    assert(false);
}
void send_keyboard_report(void) {
    report_keyboard_t report = {0};
    report_nkro_t     nkro   = {0};
    uint8_t           count  = 0;
    for (uint16_t key = 1; key < 256; key++)
        if (raw_keys[key / 8u] & (1u << (key % 8u))) {
            assert(count < sizeof(report.keys));
            report.keys[count++] = (uint8_t)key;
            nkro.bits[key / 8u] |= (uint8_t)(1u << (key % 8u));
        }
    keyboard_report_keys_filter_user(&report);
    nkro_report_keys_filter_user(&nkro);
    uint8_t projected[32] = {0};
    for (uint8_t i = 0; i < sizeof(report.keys); i++)
        if (report.keys[i]) projected[report.keys[i] / 8u] |= (uint8_t)(1u << (report.keys[i] % 8u));
    assert(memcmp(projected, nkro.bits, sizeof(nkro.bits)) == 0);
    for (uint16_t key = 1; key < 256; key++)
        if ((projected[key / 8u] & (1u << (key % 8u))) && !(visible_keys[key / 8u] & (1u << (key % 8u)))) edges[key]++;
    memcpy(visible_keys, projected, sizeof(projected));
    visible_mods = physical_mods;
    for (uint8_t i = 0; i < 8; i++)
        if (modifier_owners[i]) visible_mods |= (uint8_t)(1u << i);
    keyboard_report_mods_override_user(&visible_mods);
}
static bool visible(uint8_t key) {
    return (visible_keys[key / 8u] & (1u << (key % 8u))) != 0;
}
static void physical(uint8_t key, bool down) {
    keyrecord_t record = {.event = {.type = KEY_EVENT, .pressed = down, .key = {.row = 0, .col = 0}}};
    owned_keycode_track_physical_event(key, &record);
    if (!owned_keycode_should_suppress_default(key, &record)) {
        if (down)
            register_code(key);
        else
            unregister_code(key);
    }
}
static void drain(void) {
    for (unsigned i = 0; i < 200; i++) {
        macro_payload_debug_snapshot_t state;
        macro_payload_debug_snapshot(&state);
        if (state.state == MACRO_PAYLOAD_ENGINE_IDLE) return;
        now += 10;
        macro_payload_engine_scan();
    }
    assert(false);
}
static void reset(void) {
    macro_payload_engine_init();
    owned_keycode_reset_for_test();
    memset(raw_keys, 0, sizeof(raw_keys));
    memset(visible_keys, 0, sizeof(visible_keys));
    memset(edges, 0, sizeof(edges));
    memset(modifier_owners, 0, sizeof(modifier_owners));
    physical_mods = visible_mods = 0;
    now                          = 1000;
}
int main(void) {
    const macro_payload_ir_t text = {.protection = 1, .length = 3, .bytes = {MACRO_PAYLOAD_IR_OP_TEXT, 1, 'a'}};
    for (unsigned cancel = 0; cancel < 16; cancel++) {
        reset();
        physical_mods = MOD_BIT(KC_LEFT_CTRL);
        physical(KC_A, true);
        owned_keycode_lease_t other = {0};
        assert(owned_keycode_acquire(KC_B, &other));
        assert(visible(KC_A) && visible(KC_B));
        assert(macro_payload_start_ir(&text, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0, MACRO_PAYLOAD_SOURCE_DIRECT, 0, NULL, NULL) == MACRO_PAYLOAD_START_STARTED);
        assert(!visible(KC_A) && !visible(KC_B) && visible_mods == 0);
        for (unsigned i = 0; i < cancel; i++) {
            now += 10;
            macro_payload_engine_scan();
        }
        if (cancel < 8) macro_payload_engine_cancel();
        drain();
        assert(!visible(KC_A) && !visible(KC_B) && visible_mods == MOD_BIT(KC_LEFT_CTRL));
        owned_keycode_debug_snapshot_t ownership;
        owned_keycode_debug_snapshot(KC_A, &ownership);
        assert(ownership.physical_count == 1 && ownership.managed_count == 0);
        owned_keycode_debug_snapshot(KC_B, &ownership);
        assert(ownership.managed_count == 1 && other.active);
        if (cancel >= 8) assert(edges[KC_A] == 2u);
        physical(KC_A, false);
        physical(KC_A, true);
        assert(visible(KC_A));
        assert(owned_keycode_release(&other));
        assert(!visible(KC_B));
        physical(KC_A, false);
    }
    // A physical release during the colliding macro hold preserves its owner.
    reset();
    physical(KC_A, true);
    const macro_payload_ir_t hold = {.protection = 1, .length = 7, .bytes = {MACRO_PAYLOAD_IR_OP_KEY_DOWN, KC_A, MACRO_PAYLOAD_IR_OP_DELAY, 10, 0, MACRO_PAYLOAD_IR_OP_KEY_UP, KC_A}};
    assert(macro_payload_start_ir(&hold, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0, MACRO_PAYLOAD_SOURCE_DIRECT, 0, NULL, NULL) == MACRO_PAYLOAD_START_STARTED);
    for (unsigned i = 0; i < 4; i++) {
        now += 10;
        macro_payload_engine_scan();
    }
    assert(visible(KC_A));
    physical(KC_A, false);
    assert(visible(KC_A));
    drain();
    assert(!visible(KC_A));
    owned_keycode_debug_snapshot_t state;
    owned_keycode_debug_snapshot(KC_A, &state);
    assert(state.physical_count == 0 && state.managed_count == 0 && state.underflow_count == 0);
    puts("protected macro output preserves real ownership, collisions and earlier releases");
}
