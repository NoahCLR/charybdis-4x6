#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/rgb_validation_test"
BIN_NO_TRIGGER_HALF="$BUILD_DIR/rgb_validation_test_no_trigger_half"
RGB_FEEDBACK_TEST_FLAGS="-DRGB_PD_MODE_FEEDBACK_ENABLE -DRGB_COMBO_FEEDBACK_ENABLE -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE -DRGB_AUTOMOUSE_GRADIENT_ENABLE"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DCONSOLE_ENABLE \
    -DRGB_MATRIX_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DCOMBO_ENABLE \
    $RGB_FEEDBACK_TEST_FLAGS \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config.h" \
    "$ROOT/tests/host/rgb_validation_test.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    -o "$BIN"

"$BIN"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DCONSOLE_ENABLE \
    -DRGB_MATRIX_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DCOMBO_ENABLE \
    $RGB_FEEDBACK_TEST_FLAGS \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config_no_pd_active_half.h" \
    "$ROOT/tests/host/rgb_validation_test.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    -o "$BIN_NO_TRIGGER_HALF"

"$BIN_NO_TRIGGER_HALF"
