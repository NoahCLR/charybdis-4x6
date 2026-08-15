#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/qmk_via_sync_state_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -DVIA_ENABLE \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/qmk_via_sync_state_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_metadata.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_state.c" \
    -o "$BIN"

"$BIN"
