#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/compat/qmk_physical_half.h"

#if defined(NOAH_PHYSICAL_HALF_LEFT) || defined(NOAH_PHYSICAL_HALF_RIGHT)
bool is_keyboard_left_impl(void);
#endif

static void check(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

int main(void) {
    uint8_t origin = 0xA5u;

#if defined(NOAH_PHYSICAL_HALF_LEFT)
    check(noah_qmk_physical_half_origin(&origin), "left artifact must expose an origin");
    check(origin == NOAH_PHYSICAL_HALF_ORIGIN_LEFT, "left artifact origin must be zero");
    check(is_keyboard_left_impl(), "left artifact must override QMK handedness as left");
#elif defined(NOAH_PHYSICAL_HALF_RIGHT)
    check(noah_qmk_physical_half_origin(&origin), "right artifact must expose an origin");
    check(origin == NOAH_PHYSICAL_HALF_ORIGIN_RIGHT, "right artifact origin must be one");
    check(!is_keyboard_left_impl(), "right artifact must override QMK handedness as right");
#else
    check(!noah_qmk_physical_half_origin(&origin), "generic artifact must not invent an origin");
    check(origin == 0u, "unprovisioned origin output must be deterministic");
#endif
    check(!noah_qmk_physical_half_origin(NULL), "null output must fail closed");

    puts("qmk physical-half identity tests passed");
    return 0;
}
