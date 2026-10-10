#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -DNKRO_ENABLE \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$QMK_ROOT/platforms" -I"$QMK_ROOT/quantum/send_string" \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/macro_output_ownership_test.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_run.c" \
    "$ROOT/users/noah/lib/macro/host_layout.c" \
    "$ROOT/users/noah/lib/macro/host_layout_tables.c" \
    "$ROOT/users/noah/lib/action/owned_keycode.c" -o "$BUILD_DIR/test"
"$BUILD_DIR/test"
