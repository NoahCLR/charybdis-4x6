#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/profile_store_runtime_test"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DVIA_ENABLE \
    -DTOTAL_EEPROM_BYTE_COUNT=0x4000u \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config.h" \
    "$ROOT/tests/host/profile_store_runtime_test.c" \
    "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
    "$ROOT/users/noah/lib/profile/storage/profile_store.c" \
    "$ROOT/users/noah/lib/profile/storage/profile_store_runtime.c" \
    "$ROOT/users/noah/lib/compat/qmk_profile_eeprom.c" \
    -o "$BIN"

"$BIN"
