#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noah_real_profile_keyboard.h"
#include "print.h"
#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/noah_keymap.h"
#include "users/noah/lib/key/interaction/keymap_validation.h"
#include "users/noah/lib/pointing/defs/pd_mode_flags.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"

static char log_buffer[4096];

enum {
    TEST_PRESENT_KEY = NOAH_KEYMAP_SAFE_RANGE,
    TEST_COMBO_KEY,
    TEST_DUP_COMBO_KEY,
    TEST_DEAD_KEY,
    TEST_RAW_LAYER_ACTION = TO(LAYER_NUM),
};

static const uint16_t test_keymaps[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
    [LAYER_BASE] =
        {
            [0][0] = TEST_PRESENT_KEY,
            [0][1] = TEST_RAW_LAYER_ACTION,
        },
};

static const uint16_t combo_outputs[] = {
    TEST_COMBO_KEY,
    MO(LAYER_SYM),
    TEST_DUP_COMBO_KEY,
};

static const uint16_t combo_keys_0[] = {
    KC_A,
    KC_B,
    COMBO_END,
};

static const uint16_t combo_keys_1[] = {
    KC_C,
    KC_D,
    COMBO_END,
};

static const uint16_t combo_keys_2[] = {
    KC_Z,
    KC_Z,
    COMBO_END,
};

const key_behavior_t key_behaviors[] = {
    {.keycode = TEST_PRESENT_KEY},
    {.keycode = TEST_COMBO_KEY},
    {.keycode = TEST_DEAD_KEY},
};

combo_t key_combos[] = {
    {.keys = combo_keys_0, .keycode = TEST_COMBO_KEY},
    {.keys = combo_keys_1, .keycode = MO(LAYER_SYM)},
    {.keys = combo_keys_2, .keycode = TEST_DUP_COMBO_KEY},
};

const uint8_t         key_behavior_count         = ARRAY_SIZE(key_behaviors);
const uint8_t         noah_combo_count           = ARRAY_SIZE(key_combos);
const uint16_t *const noah_combo_output_keycodes = combo_outputs;
const uint8_t         noah_combo_output_count    = ARRAY_SIZE(combo_outputs);

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

int uprintf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int written = vsnprintf(log_buffer + strlen(log_buffer), sizeof(log_buffer) - strlen(log_buffer), fmt, args);
    va_end(args);

    return written;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
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

bool pd_mode_toggle_lock_state_at(pd_mode_mask_t mode, keypos_t key_pos) {
    (void)key_pos;
    return pd_mode_toggle_lock_state(mode);
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

void register_code16(uint16_t keycode) {
    (void)keycode;
}

void tap_code16(uint16_t keycode) {
    (void)keycode;
}

void unregister_code16(uint16_t keycode) {
    (void)keycode;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return test_keymaps[layer_num][row][column];
}

int main(void) {
    char combo_key_hex[16];
    char dead_key_hex[16];

    snprintf(combo_key_hex, sizeof(combo_key_hex), "0x%04X", (unsigned int)TEST_COMBO_KEY);
    snprintf(dead_key_hex, sizeof(dead_key_hex), "0x%04X", (unsigned int)TEST_DEAD_KEY);

    noah_keymap_validate();

    CHECK(strstr(log_buffer, "Unsupported keymaps[0][0][1] raw layer action") != NULL);
    CHECK(strstr(log_buffer, "Unsupported COMBOS(COMBO) input[2] duplicate member keycode") != NULL);
    CHECK(strstr(log_buffer, "Unsupported COMBOS(COMBO) output[1] raw layer action") != NULL);
    CHECK(strstr(log_buffer, "Unreachable key_behaviors[2].keycode") != NULL);
    CHECK(strstr(log_buffer, dead_key_hex) != NULL);
    CHECK(strstr(log_buffer, combo_key_hex) == NULL);

    puts("keymap_validation host tests passed");
    return 0;
}
