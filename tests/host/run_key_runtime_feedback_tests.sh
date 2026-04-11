#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/key_runtime_feedback_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_feedback_test.c" \
    "$ROOT/users/noah/lib/key/key_runtime_feedback.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_effect.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_press.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_release.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_result.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot_scan.c" \
    "$ROOT/users/noah/lib/key/key_runtime_slot.c" \
    "$ROOT/users/noah/lib/key/multi_tap_engine.c" \
    "$ROOT/users/noah/lib/state/runtime_shared_state.c" \
    -o "$BIN"

"$BIN"
