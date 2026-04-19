#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/key_runtime_modifier_hold_integration_test"

. "$ROOT/tests/host/noah_source_manifest.sh"
MODIFIER_HOLD_SUPPORT_SOURCES="$(noah_host_key_runtime_modifier_hold_support_paths "$ROOT")"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# Intentional word splitting for derived support source list.
# shellcheck disable=SC2086
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_integration_harness.c" \
    "$ROOT/tests/host/key_runtime_modifier_hold_integration_test.c" \
    "$ROOT/tests/host/runtime_v2_observer_stub.c" \
    $MODIFIER_HOLD_SUPPORT_SOURCES \
    -o "$BIN"

"$BIN"
