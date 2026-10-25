#!/bin/sh

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/pointer_layer_policy_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DNOAH_PORTABLE_PROFILE_ENABLE \
    -DCHARYBDIS_AUTO_SNIPING_LAYER=2 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/pointer_layer_policy_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/pointing/policy/pointer_layer_policy.c" \
    -o "$BIN"

"$BIN"
