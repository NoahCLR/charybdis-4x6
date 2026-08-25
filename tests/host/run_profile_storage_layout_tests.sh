#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/profile_storage_layout_test"
PROBE_OBJECT="$BUILD_DIR/profile_storage_layout_probe.o"
PROBE_LOG="$BUILD_DIR/profile_storage_layout_probe.log"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

compile_layout() {
    total_eeprom_bytes="$1"
    via_config_end="$2"
    shift 2

    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -DVIA_ENABLE \
        -DMCU_RP \
        -DRGB_MATRIX_ENABLE \
        -DMATRIX_ROWS=10 \
        -DMATRIX_COLS=6 \
        -DTOTAL_EEPROM_BYTE_COUNT="$total_eeprom_bytes" \
        -DVIA_EEPROM_CONFIG_END="$via_config_end" \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        "$@"
}

compile_layout 16384u 41u \
    "$ROOT/tests/host/profile_storage_layout_test.c" \
    "$ROOT/users/noah/lib/profile/storage/profile_storage_layout.c" \
    -o "$BIN"

"$BIN"

expect_contract_compile_failure() {
    label="$1"
    total_eeprom_bytes="$2"
    via_config_end="$3"

    if compile_layout "$total_eeprom_bytes" "$via_config_end" \
        -c "$ROOT/users/noah/lib/profile/storage/profile_storage_layout.c" \
        -o "$PROBE_OBJECT" >"$PROBE_LOG" 2>&1; then
        echo "expected profile storage layout compile failure: $label" >&2
        exit 1
    fi
}

expect_contract_compile_failure "logical EEPROM size drift" 8192u 41u
expect_contract_compile_failure "VIA config boundary drift" 16384u 40u

echo "profile storage layout compile guards passed"
