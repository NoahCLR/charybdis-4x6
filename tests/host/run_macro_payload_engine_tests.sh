#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/macro_payload_engine_test"
SANITIZED_BIN="$BUILD_DIR/macro_payload_engine_sanitized_test"

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
    "$ROOT/tests/host/macro_payload_engine_test.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_run.c" \
    -o "$BIN"

"$BIN"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$QMK_ROOT/platforms" \
    -I"$QMK_ROOT/quantum/send_string" \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/macro_payload_engine_test.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_run.c" \
    -o "$SANITIZED_BIN"

"$SANITIZED_BIN"

if rg -n '\b(wait_ms|send_char|send_char_with_delay|owned_keycode_tap)[[:space:]]*\(' \
    "$ROOT/users/noah/lib/macro/macro_payload_run.c"; then
    echo "blocking helper reintroduced into firmware macro playback" >&2
    exit 1
fi
