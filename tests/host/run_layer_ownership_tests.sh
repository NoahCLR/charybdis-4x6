#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/layer_ownership_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DNOAH_HOST_TESTS \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/layer_ownership_test.c" \
    "$ROOT/users/noah/lib/state/ownership/layer_ownership.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_shared_state.c" \
    "$ROOT/tests/host/runtime_v2_state_unit_stub.c" \
    -o "$BIN"

"$BIN"
