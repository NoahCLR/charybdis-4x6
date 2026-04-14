#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/runtime_debug_test"

. "$ROOT/tests/host/noah_source_manifest.sh"
SHARED_SOURCES="$(noah_host_public_key_runtime_support_paths "$ROOT")"
RUNTIME_DEBUG_EXTRA_SOURCES="
$ROOT/users/noah/lib/key/runtime/key_runtime_feedback.c
$ROOT/users/noah/lib/pointing/runtime/pd_mode_snapshot.c
$ROOT/users/noah/lib/pointing/runtime/pd_mode_state.c
$ROOT/users/noah/lib/state/ownership/layer_ownership.c
$ROOT/users/noah/lib/state/runtime/runtime_trace.c
"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# Intentional word splitting for shared and runner-local source lists.
# shellcheck disable=SC2086
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_TRACE_ENABLE \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/runtime_debug_test.c" \
    $SHARED_SOURCES \
    $RUNTIME_DEBUG_EXTRA_SOURCES \
    -o "$BIN"

"$BIN"
