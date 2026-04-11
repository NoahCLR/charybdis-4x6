#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/keymap_validation_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DCONSOLE_ENABLE \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config.h" \
    "$ROOT/tests/host/keymap_validation_test.c" \
    "$ROOT/users/noah/lib/key/key_behavior_lookup.c" \
    "$ROOT/users/noah/lib/key/keymap_validation.c" \
    -o "$BIN"

"$BIN"
