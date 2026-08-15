#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/qmk_via_split_sync_test"
SANITIZED_BIN="$BUILD_DIR/qmk_via_split_sync_sanitized_test"
ENCODER_BIN="$BUILD_DIR/qmk_via_split_sync_encoder_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DVIA_ENABLE \
    -DVIA_EEPROM_ALLOW_RESET \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/qmk_via_split_sync_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_metadata.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_protocol.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_state.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_split_sync.c" \
    -o "$BIN"

"$BIN"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer \
    -DVIA_ENABLE \
    -DVIA_EEPROM_ALLOW_RESET \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/qmk_via_split_sync_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_metadata.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_protocol.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_state.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_split_sync.c" \
    -o "$SANITIZED_BIN"

"$SANITIZED_BIN"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DVIA_ENABLE \
    -DVIA_EEPROM_ALLOW_RESET \
    -DENCODER_MAP_ENABLE \
    -DNUM_ENCODERS=2 \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/qmk_via_split_sync_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_metadata.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_protocol.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_state.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_split_sync.c" \
    -o "$ENCODER_BIN"

"$ENCODER_BIN"
