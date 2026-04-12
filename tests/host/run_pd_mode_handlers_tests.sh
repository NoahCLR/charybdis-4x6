#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/pd_mode_handlers_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DPOINTING_DEVICE_ENABLE \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config_no_automouse.h" \
    "$ROOT/tests/host/pd_mode_handlers_test.c" \
    "$ROOT/users/noah/lib/pointing/pd_mode_dragscroll.c" \
    "$ROOT/users/noah/lib/pointing/pd_mode_volume.c" \
    "$ROOT/users/noah/lib/pointing/pd_mode_brightness.c" \
    "$ROOT/users/noah/lib/pointing/pd_mode_zoom.c" \
    "$ROOT/users/noah/lib/pointing/pd_mode_arrow.c" \
    "$ROOT/users/noah/lib/state/keyboard_mod_state.c" \
    -o "$BIN"

"$BIN"
