#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/owned_keycode.h"

static uint8_t  register_code_calls[16];
static uint8_t  unregister_code_calls[16];
static uint8_t  register_code_count;
static uint8_t  unregister_code_count;
static uint16_t mod_register_calls[16];
static uint16_t mod_unregister_calls[16];
static uint8_t  mod_register_count;
static uint8_t  mod_unregister_count;
static uint8_t  register_mods_calls[16];
static uint8_t  unregister_mods_calls[16];
static uint8_t  register_mods_count;
static uint8_t  unregister_mods_count;
static uint16_t wait_calls[16];
static uint8_t  wait_call_count;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)            \
    do {                       \
        if (!(expr)) {         \
            test_fail(#expr, __FILE__, __LINE__); \
        }                      \
    } while (0)

static void test_reset_stubs(void) {
    register_code_count   = 0;
    unregister_code_count = 0;
    mod_register_count    = 0;
    mod_unregister_count  = 0;
    register_mods_count   = 0;
    unregister_mods_count = 0;
    wait_call_count       = 0;
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    CHECK(mod_register_count < ARRAY_SIZE(mod_register_calls));
    mod_register_calls[mod_register_count++] = keycode;
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    CHECK(mod_unregister_count < ARRAY_SIZE(mod_unregister_calls));
    mod_unregister_calls[mod_unregister_count++] = keycode;
}

void keyboard_mod_ownership_register_mods(uint8_t mods) {
    CHECK(register_mods_count < ARRAY_SIZE(register_mods_calls));
    register_mods_calls[register_mods_count++] = mods;
}

void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    CHECK(unregister_mods_count < ARRAY_SIZE(unregister_mods_calls));
    unregister_mods_calls[unregister_mods_count++] = mods;
}

void register_code(uint8_t keycode) {
    CHECK(register_code_count < ARRAY_SIZE(register_code_calls));
    register_code_calls[register_code_count++] = keycode;
}

void unregister_code(uint8_t keycode) {
    CHECK(unregister_code_count < ARRAY_SIZE(unregister_code_calls));
    unregister_code_calls[unregister_code_count++] = keycode;
}

void wait_ms(uint16_t ms) {
    CHECK(wait_call_count < ARRAY_SIZE(wait_calls));
    wait_calls[wait_call_count++] = ms;
}

static void test_plain_key_registers_and_unregisters_directly(void) {
    test_reset_stubs();

    CHECK(owned_keycode_register(KC_C));
    CHECK(register_code_count == 1);
    CHECK(register_code_calls[0] == KC_C);
    CHECK(mod_register_count == 0);
    CHECK(register_mods_count == 0);

    CHECK(owned_keycode_unregister(KC_C));
    CHECK(unregister_code_count == 1);
    CHECK(unregister_code_calls[0] == KC_C);
    CHECK(mod_unregister_count == 0);
    CHECK(unregister_mods_count == 0);
}

static void test_plain_modifier_uses_mod_ownership(void) {
    test_reset_stubs();

    CHECK(owned_keycode_register(KC_LEFT_SHIFT));
    CHECK(mod_register_count == 1);
    CHECK(mod_register_calls[0] == KC_LEFT_SHIFT);
    CHECK(register_code_count == 0);

    CHECK(owned_keycode_unregister(KC_LEFT_SHIFT));
    CHECK(mod_unregister_count == 1);
    CHECK(mod_unregister_calls[0] == KC_LEFT_SHIFT);
    CHECK(unregister_code_count == 0);
}

static void test_modded_key_registers_mods_and_basic_key(void) {
    uint16_t keycode = G(KC_V);

    test_reset_stubs();

    CHECK(owned_keycode_register(keycode));
    CHECK(register_mods_count == 1);
    CHECK(register_mods_calls[0] == MOD_BIT(KC_LEFT_GUI));
    CHECK(register_code_count == 1);
    CHECK(register_code_calls[0] == KC_V);

    CHECK(owned_keycode_unregister(keycode));
    CHECK(unregister_code_count == 1);
    CHECK(unregister_code_calls[0] == KC_V);
    CHECK(unregister_mods_count == 1);
    CHECK(unregister_mods_calls[0] == MOD_BIT(KC_LEFT_GUI));
}

static void test_right_modded_key_uses_right_side_modifier_mask(void) {
    uint16_t keycode = (uint16_t)(QK_RMODS_MIN | QK_LSFT | KC_C);

    test_reset_stubs();

    CHECK(owned_keycode_register(keycode));
    CHECK(register_mods_count == 1);
    CHECK(register_mods_calls[0] == MOD_BIT(KC_RIGHT_SHIFT));
    CHECK(register_code_count == 1);
    CHECK(register_code_calls[0] == KC_C);

    CHECK(owned_keycode_unregister(keycode));
    CHECK(unregister_mods_count == 1);
    CHECK(unregister_mods_calls[0] == MOD_BIT(KC_RIGHT_SHIFT));
    CHECK(unregister_code_count == 1);
    CHECK(unregister_code_calls[0] == KC_C);
}

static void test_non_8bit_non_modded_keycodes_are_rejected(void) {
    test_reset_stubs();

    CHECK(!owned_keycode_register(SAFE_RANGE));
    CHECK(!owned_keycode_unregister(SAFE_RANGE));
    CHECK(register_code_count == 0);
    CHECK(unregister_code_count == 0);
    CHECK(mod_register_count == 0);
    CHECK(mod_unregister_count == 0);
}

static void test_tap_uses_standard_and_caps_delays(void) {
    test_reset_stubs();

    CHECK(owned_keycode_tap(KC_C));
    CHECK(register_code_count == 1);
    CHECK(unregister_code_count == 1);
    CHECK(wait_call_count == 1);
    CHECK(wait_calls[0] == TAP_CODE_DELAY);

    test_reset_stubs();

    CHECK(owned_keycode_tap(KC_CAPS_LOCK));
    CHECK(register_code_count == 1);
    CHECK(unregister_code_count == 1);
    CHECK(wait_call_count == 1);
    CHECK(wait_calls[0] == TAP_HOLD_CAPS_DELAY);
}

int main(void) {
    test_plain_key_registers_and_unregisters_directly();
    test_plain_modifier_uses_mod_ownership();
    test_modded_key_registers_mods_and_basic_key();
    test_right_modded_key_uses_right_side_modifier_mask();
    test_non_8bit_non_modded_keycodes_are_rejected();
    test_tap_uses_standard_and_caps_delays();

    puts("owned_keycode host tests passed");
    return 0;
}
