#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -pedantic "$@" \
        -I"$ROOT" \
        "$ROOT/tests/host/profile_wire_v1_test.c" \
        "$ROOT/users/noah/lib/profile/protocol/profile_wire_v1.c" \
        -o "$BUILD_DIR/profile_wire_v1_test_$name"
    "$BUILD_DIR/profile_wire_v1_test_$name" "$ROOT/tests/fixtures/profile_wire_v1_reads.fixture"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic -fsyntax-only \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config.h" \
    "$ROOT/users/noah/lib/compat/qmk_via_profile_channel.c"

build_hook_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DVIA_ENABLE \
        "$@" \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        "$ROOT/tests/host/qmk_via_profile_channel_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_via_profile_channel.c" \
        "$ROOT/users/noah/lib/profile/protocol/profile_wire_v1.c" \
        -o "$BUILD_DIR/qmk_via_profile_channel_test_$name"
    "$BUILD_DIR/qmk_via_profile_channel_test_$name"
}

build_hook_and_run via
build_hook_and_run via_rgb -DRGB_MATRIX_ENABLE -DRGB_MATRIX_WS2812
build_hook_and_run via_split -DSPLIT_KEYBOARD
build_hook_and_run via_split_rgb -DSPLIT_KEYBOARD -DRGB_MATRIX_ENABLE -DRGB_MATRIX_WS2812
