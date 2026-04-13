#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "print.h"
#include "users/noah/noah_keymap.h"
#include "users/noah/lib/key/interaction/keymap_validation.h"
#include "users/noah/lib/pointing/defs/pd_mode_flags.h"

static char log_buffer[4096];

enum {
    TEST_PRESENT_KEY = NOAH_KEYMAP_SAFE_RANGE,
    TEST_COMBO_KEY,
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
};

const key_behavior_t key_behaviors[] = {
    {.keycode = TEST_PRESENT_KEY},
    {.keycode = TEST_COMBO_KEY},
    {.keycode = TEST_DEAD_KEY},
};

const uint8_t         key_behavior_count         = ARRAY_SIZE(key_behaviors);
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
    CHECK(strstr(log_buffer, "Unsupported COMBOS(COMBO) output[1] raw layer action") != NULL);
    CHECK(strstr(log_buffer, "Unreachable key_behaviors[2].keycode") != NULL);
    CHECK(strstr(log_buffer, dead_key_hex) != NULL);
    CHECK(strstr(log_buffer, combo_key_hex) == NULL);

    puts("keymap_validation host tests passed");
    return 0;
}
