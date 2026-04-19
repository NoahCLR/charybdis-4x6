#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/key_runtime_admission_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_admission_test.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_defaults.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_transparency.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_materialize.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_admission.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_index.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot.c" \
    "$ROOT/users/noah/lib/key/interaction/multi_tap_engine.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_shared_state.c" \
    "$ROOT/tests/host/runtime_v2_observer_stub.c" \
    -o "$BIN"

"$BIN"
