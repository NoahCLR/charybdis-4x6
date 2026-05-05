#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/rgb_validation_test"
BIN_NO_TRIGGER_HALF="$BUILD_DIR/rgb_validation_test_no_trigger_half"
RGB_FEEDBACK_CONFIG="$BUILD_DIR/noah_compile_config_rgb_feedback.h"
RGB_FEEDBACK_NO_TRIGGER_HALF_CONFIG="$BUILD_DIR/noah_compile_config_rgb_feedback_no_trigger_half.h"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cat >"$RGB_FEEDBACK_CONFIG" <<EOF
#include "$ROOT/tests/host/include/noah_compile_config.h"
#ifndef RGB_PD_MODE_FEEDBACK_ENABLE
#    define RGB_PD_MODE_FEEDBACK_ENABLE
#endif
#ifndef RGB_COMBO_FEEDBACK_ENABLE
#    define RGB_COMBO_FEEDBACK_ENABLE
#endif
#ifndef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#    define RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#endif
#ifndef RGB_AUTOMOUSE_GRADIENT_ENABLE
#    define RGB_AUTOMOUSE_GRADIENT_ENABLE
#endif
EOF

cat >"$RGB_FEEDBACK_NO_TRIGGER_HALF_CONFIG" <<EOF
#include "$RGB_FEEDBACK_CONFIG"
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
#    undef RGB_PD_MODE_ACTIVE_HALF_ENABLE
#endif
EOF

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DCONSOLE_ENABLE \
    -DRGB_MATRIX_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DCOMBO_ENABLE \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$RGB_FEEDBACK_CONFIG" \
    "$ROOT/tests/host/rgb_validation_test.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    -o "$BIN"

"$BIN"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DCONSOLE_ENABLE \
    -DRGB_MATRIX_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DCOMBO_ENABLE \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$RGB_FEEDBACK_NO_TRIGGER_HALF_CONFIG" \
    "$ROOT/tests/host/rgb_validation_test.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    -o "$BIN_NO_TRIGGER_HALF"

"$BIN_NO_TRIGGER_HALF"
