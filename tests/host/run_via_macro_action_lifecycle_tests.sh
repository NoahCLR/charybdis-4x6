#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/via_macro_action_lifecycle_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DVIA_ENABLE \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/via_macro_action_lifecycle_test.c" \
    "$ROOT/users/noah/lib/macro/macro_slot_provider.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/macro/macro_payload.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_decode_qmk.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_encode.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_keycodes.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_parse.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_run.c" \
    "$ROOT/users/noah/lib/macro/via_macro_provider.c" \
    "$ROOT/users/noah/lib/action/action_lifecycle.c" \
    -o "$BIN"

"$BIN"
