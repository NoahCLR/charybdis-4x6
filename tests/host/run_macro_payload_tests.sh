#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
QMK_ROOT="${QMK_ROOT:-$ROOT/../bastardkb-qmk}"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/macro_payload_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$QMK_ROOT/platforms" \
    -I"$QMK_ROOT/quantum/send_string" \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/macro_payload_test.c" \
    "$ROOT/users/noah/lib/macro/macro_payload.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_decode_qmk.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_keycodes.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_parse.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_run.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_encode.c" \
    -o "$BIN"

"$BIN"
