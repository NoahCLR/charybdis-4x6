#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/key_runtime_release_matrix_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_scenario_harness.c" \
    "$ROOT/tests/host/key_runtime_release_matrix_test.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_trace.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_process.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_admission.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_release_reduce.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_result.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_scan_reduce.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_step.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_preflight.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_press.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_release.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_scan.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_transition.c" \
    "$ROOT/users/noah/lib/key/interaction/multi_tap_engine.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_debug.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_shared_state.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_trace.c" \
    -o "$BIN"

"$BIN"
echo "key_runtime_release_matrix host tests passed"
