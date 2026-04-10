#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

COMMON_SOURCES="
users/noah/runtime_init.c
users/noah/lib/action/action_lifecycle.c
users/noah/lib/macro/via_macro_defaults.c
users/noah/lib/state/split_runtime_sync.c
users/noah/lib/rgb/rgb_runtime.c
"

POINTING_SOURCES="
users/noah/lib/pointing/pd_runtime.c
users/noah/lib/pointing/pd_mode_state.c
users/noah/lib/pointing/pd_mode_registry.c
users/noah/lib/pointing/pointer_layer_policy.c
users/noah/lib/pointing/pd_mode_handlers.c
"

RGB_SOURCES="
keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c
"

AUTOMOUSE_SOURCES="
users/noah/lib/rgb/rgb_automouse.c
"

POINTING_TEST_FLAGS="-DPOINTING_DEVICE_ENABLE -DDPI_MOD=0x5201u -DDPI_RMOD=0x5202u -DS_D_MOD=0x5203u -DS_D_RMOD=0x5204u"
RGB_TEST_FLAGS="-DRGB_MATRIX_ENABLE -DRGB_MATRIX_WS2812"

compile_variant() {
    config_header="$1"
    extra_flags="$2"
    sources="$3"

    # Intentional word splitting for flag and source lists.
    # shellcheck disable=SC2086
    for src in $sources; do
        cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic -fsyntax-only \
            -DQMK_KEYBOARD_H='"qmk_stub.h"' \
            -DQMK_STUB_SUPPRESS_LAYER_COUNT \
            $extra_flags \
            -I"$ROOT" \
            -I"$ROOT/users/noah" \
            -I"$ROOT/tests/host/include" \
            -include "$ROOT/$config_header" \
            "$ROOT/$src"
    done
}

compile_variant "tests/host/include/noah_compile_config.h" "" "$COMMON_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_rgb_feedback.h" "$RGB_TEST_FLAGS" "$COMMON_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config.h" "$RGB_TEST_FLAGS" "$COMMON_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "$POINTING_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "$POINTING_TEST_FLAGS $RGB_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config_no_automouse.h" "-DSPLIT_KEYBOARD $POINTING_TEST_FLAGS $RGB_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES $RGB_SOURCES"
compile_variant "tests/host/include/noah_compile_config.h" "-DSPLIT_KEYBOARD $POINTING_TEST_FLAGS $RGB_TEST_FLAGS" "$COMMON_SOURCES $POINTING_SOURCES $RGB_SOURCES $AUTOMOUSE_SOURCES"
