#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/key_runtime_layer_lock_integration_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_HOST_TESTS \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_integration_harness.c" \
    "$ROOT/tests/host/key_runtime_layer_lock_integration_test.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/action/action_kind_dispatch.c" \
    "$ROOT/users/noah/lib/action/action_dispatch.c" \
    "$ROOT/users/noah/lib/action/action_lifecycle.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_defaults.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_resolution_accessors.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_lookup.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_transparency.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_materialize.c" \
    "$ROOT/users/noah/lib/key/interaction/multi_tap_engine.c" \
    "$ROOT/users/noah/lib/key/runtime/api.c" \
    "$ROOT/users/noah/lib/key/runtime/debug.c" \
    "$ROOT/users/noah/lib/key/runtime/preflight.c" \
    "$ROOT/users/noah/lib/key/runtime/press.c" \
    "$ROOT/users/noah/lib/key/runtime/process.c" \
    "$ROOT/users/noah/lib/key/runtime/release.c" \
    "$ROOT/users/noah/lib/key/runtime/scan.c" \
    "$ROOT/users/noah/lib/key/runtime/trace.c" \
    "$ROOT/users/noah/lib/key/runtime/transition.c" \
    "$ROOT/users/noah/lib/key/runtime/core/runtime.c" \
    "$ROOT/users/noah/lib/state/ownership/layer_ownership.c" \
    "$ROOT/users/noah/lib/state/runtime/keyboard_mod_state.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_diag.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_shared_state.c" \
    -o "$BIN"

"$BIN"
