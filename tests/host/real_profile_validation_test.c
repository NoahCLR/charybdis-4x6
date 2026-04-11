#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "print.h"
#include "users/noah/noah_keymap.h"
#include "users/noah/noah_runtime.h"
#include "users/noah/lib/key/keymap_validation.h"
#include "users/noah/lib/pointing/pd_modes.h"
#include "users/noah/lib/rgb/rgb_validation.h"

static char log_buffer[16384];

extern const pd_mode_color_t pd_mode_colors[];
extern const uint8_t         pd_mode_color_count;

#define NOAH_PD_MODE_TEST_ROW(name, mode_keycode, handler, key_handler, reset, dpi, mode_traits) [PD_MODE_INDEX_##name] = {.mode_flag = PD_MODE_##name, .keycode = (mode_keycode), .lock_action = mode_keycode##_LOCK, .traits = (mode_traits)},
const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {NOAH_PD_MODE_LIST(NOAH_PD_MODE_TEST_ROW)};
#undef NOAH_PD_MODE_TEST_ROW

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

bool action_dispatch_is_raw_qmk_layer_action(uint16_t action) {
    return IS_QK_MOMENTARY(action) || IS_QK_LAYER_TAP(action);
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode == keycode) {
            return pd_modes[i].mode_flag;
        }
    }

    return 0;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return keymaps[layer_num][row][column];
}

int main(void) {
    CHECK(key_behavior_count > 0);
    CHECK(noah_combo_output_count > 0);
    CHECK(pd_mode_color_count == PD_MODE_COUNT);

    noah_keymap_validate();
    noah_rgb_validate_config();

    if (log_buffer[0] != '\0') {
        fputs(log_buffer, stderr);
        test_fail("log_buffer is empty", __FILE__, __LINE__);
    }

    puts("real profile validation host tests passed");
    return 0;
}
