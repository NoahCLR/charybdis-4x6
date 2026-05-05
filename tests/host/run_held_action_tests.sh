#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/held_action_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/held_action_test.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/key/ownership/held_action.c" \
    "$ROOT/users/noah/lib/key/ownership/held_repeat.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    "$ROOT/users/noah/lib/state/shared/runtime_shared_state.c" \
    -o "$BIN"

"$BIN"
