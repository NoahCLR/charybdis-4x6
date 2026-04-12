#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/key_runtime_slot_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_slot_test.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_policy.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_pending_multi_tap.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_result.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_step.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot.c" \
    "$ROOT/users/noah/lib/key/multi_tap_engine.c" \
    "$ROOT/users/noah/lib/state/runtime_shared_state.c" \
    -o "$BIN"

"$BIN"
