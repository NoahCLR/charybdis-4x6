#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/pointer_layer_policy_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DCHARYBDIS_AUTO_SNIPING_LAYER=2 \
    -DDPI_MOD=0x5201u \
    -DDPI_RMOD=0x5202u \
    -DS_D_MOD=0x5203u \
    -DS_D_RMOD=0x5204u \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/pointer_layer_policy_test.c" \
    "$ROOT/users/noah/lib/pointing/pointer_layer_policy.c" \
    -o "$BIN"

"$BIN"
