#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/key_runtime_modifier_hold_integration_test"

. "$ROOT/tests/host/noah_source_manifest.sh"
SHARED_SOURCES="$(noah_host_public_key_runtime_support_paths "$ROOT")"
MODIFIER_HOLD_EXTRA_SOURCES="
$ROOT/users/noah/lib/action/owned_keycode.c
$ROOT/users/noah/lib/key/interaction/handled_key_lookup.c
$ROOT/users/noah/lib/key/runtime/key_runtime.c
$ROOT/users/noah/lib/key/runtime/key_runtime_scan.c
"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# Intentional word splitting for shared and runner-local source lists.
# shellcheck disable=SC2086
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_integration_harness.c" \
    "$ROOT/tests/host/key_runtime_modifier_hold_integration_test.c" \
    $SHARED_SOURCES \
    $MODIFIER_HOLD_EXTRA_SOURCES \
    -o "$BIN"

"$BIN"
