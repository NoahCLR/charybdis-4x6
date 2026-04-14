#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/runtime_debug_test"

. "$ROOT/tests/host/noah_source_manifest.sh"
RUNTIME_DEBUG_SUPPORT_SOURCES="$(noah_host_runtime_debug_support_paths "$ROOT")"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# Intentional word splitting for derived support source list.
# shellcheck disable=SC2086
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_TRACE_ENABLE \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/runtime_debug_test.c" \
    $RUNTIME_DEBUG_SUPPORT_SOURCES \
    -o "$BIN"

"$BIN"
