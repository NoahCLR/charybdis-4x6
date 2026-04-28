#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/delayed_action_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/delayed_action_test.c" \
    "$ROOT/users/noah/lib/key/runtime/delayed_action.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/action/action_dispatch.c" \
    "$ROOT/users/noah/lib/state/modifiers/keyboard_mod_state.c" \
    "$ROOT/users/noah/lib/state/modifiers/keyboard_mod_policy.c" \
    -o "$BIN"

"$BIN"
