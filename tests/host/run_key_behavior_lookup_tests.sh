#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/key_behavior_lookup_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DKEY_BEHAVIOR_LOOKUP_TEST_INSTRUMENTATION \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_behavior_lookup_test.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/key/behavior/key_behavior_lookup.c" \
    "$ROOT/users/noah/lib/key/behavior/handled_key_defaults.c" \
    "$ROOT/users/noah/lib/key/behavior/handled_key_resolution_accessors.c" \
    "$ROOT/users/noah/lib/key/behavior/handled_key_lookup.c" \
    "$ROOT/users/noah/lib/key/behavior/handled_key_transparency.c" \
    "$ROOT/users/noah/lib/key/behavior/handled_key_materialize.c" \
    -o "$BIN"

"$BIN"
