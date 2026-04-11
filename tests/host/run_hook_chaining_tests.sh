#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN_DEFAULT="$BUILD_DIR/hook_chaining_test"
BIN_OVERRIDE="$BUILD_DIR/hook_chaining_override_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/hook_chaining_test.c" \
    "$ROOT/users/noah/hooks.c" \
    -o "$BIN_DEFAULT"

"$BIN_DEFAULT"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DHOOK_CHAINING_TEST_STRONG_OVERRIDE \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/hook_chaining_test.c" \
    "$ROOT/users/noah/hooks.c" \
    -o "$BIN_OVERRIDE"

"$BIN_OVERRIDE"

echo "hook chaining host tests passed"
