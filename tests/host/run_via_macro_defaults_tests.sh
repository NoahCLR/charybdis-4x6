#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/via_macro_defaults_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DVIA_ENABLE \
    -DVIA_EEPROM_ALLOW_RESET \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/via_macro_defaults_test.c" \
    "$ROOT/users/noah/lib/macro/macro_slot_provider.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_decode_qmk.c" \
    "$ROOT/users/noah/lib/macro/via_macro_provider.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_contract.c" \
    "$ROOT/users/noah/lib/macro/via_macro_defaults.c" \
    -o "$BIN"

"$BIN"
