#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/key/interaction/key_behavior_lookup.h"
#include "users/noah/lib/pointing/defs/pd_mode_flags.h"

static char log_buffer[1024];

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

const key_behavior_t key_behaviors[] = {
    {.keycode = KC_C},
    {.keycode = KC_C},
};

const uint8_t key_behavior_count = ARRAY_SIZE(key_behaviors);

int uprintf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int written = vsnprintf(log_buffer + strlen(log_buffer), sizeof(log_buffer) - strlen(log_buffer), fmt, args);
    va_end(args);

    return written;
}

bool action_dispatch_is_raw_qmk_layer_action(uint16_t action) {
    (void)action;
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    (void)keycode;
    return 0;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

int main(void) {
    key_behavior_validate_all();

    CHECK(strstr(log_buffer, "Duplicate key_behaviors rows") != NULL);
    CHECK(strstr(log_buffer, "later rows are ignored by lookup") != NULL);

    puts("key_behavior_validation host tests passed");
    return 0;
}
