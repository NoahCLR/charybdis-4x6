#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/key_runtime_modifier_hold_integration_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_modifier_hold_integration_test.c" \
    "$ROOT/users/noah/lib/action/owned_keycode.c" \
    "$ROOT/users/noah/lib/key/key_runtime.c" \
    "$ROOT/users/noah/lib/key/key_runtime_admission.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_policy.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_pending_multi_tap.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_release_reduce.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_result.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_scan_reduce.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_step.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot.c" \
    "$ROOT/users/noah/lib/key/key_runtime_trace.c" \
    "$ROOT/users/noah/lib/key/key_runtime_press.c" \
    "$ROOT/users/noah/lib/key/key_runtime_release.c" \
    "$ROOT/users/noah/lib/key/key_runtime_scan.c" \
    "$ROOT/users/noah/lib/key/key_runtime_transition.c" \
    "$ROOT/users/noah/lib/key/multi_tap_engine.c" \
    "$ROOT/users/noah/lib/key/held_action.c" \
    "$ROOT/users/noah/lib/state/keyboard_mod_ownership.c" \
    "$ROOT/users/noah/lib/state/runtime_shared_state.c" \
    -o "$BIN"

"$BIN"
