#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/pd_mode_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DPOINTING_DEVICE_ENABLE \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DPD_MODE_VOLUME_DPI=1400 \
    -DPD_MODE_BRIGHTNESS_DPI=1500 \
    -DPD_MODE_ZOOM_DPI=1600 \
    -DPD_MODE_ARROW_DPI=1700 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/pd_mode_test.c" \
    "$ROOT/users/noah/lib/pointing/pd_mode_registry.c" \
    "$ROOT/users/noah/lib/pointing/pd_mode_state.c" \
    "$ROOT/users/noah/lib/state/runtime_shared_state.c" \
    -o "$BIN"

"$BIN"
