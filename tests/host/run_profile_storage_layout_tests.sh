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
    eeconfig_size="$2"
    shift 2

    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -DMCU_RP \
        -DRGB_MATRIX_ENABLE \
        -DMATRIX_ROWS=10 \
        -DMATRIX_COLS=6 \
        -DTOTAL_EEPROM_BYTE_COUNT="$total_eeprom_bytes" \
        -DEECONFIG_SIZE="$eeconfig_size" \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        "$@"
}

compile_layout 4096u 41u \
    "$ROOT/tests/host/profile_storage_layout_test.c" \
    "$ROOT/users/noah/lib/profile/storage/profile_storage_layout.c" \
    -o "$BIN"

"$BIN"

expect_contract_compile_failure() {
    label="$1"
    total_eeprom_bytes="$2"
    eeconfig_size="$3"

    if compile_layout "$total_eeprom_bytes" "$eeconfig_size" \
        -c "$ROOT/users/noah/lib/profile/storage/profile_storage_layout.c" \
        -o "$PROBE_OBJECT" >"$PROBE_LOG" 2>&1; then
        echo "expected profile storage layout compile failure: $label" >&2
        exit 1
    fi
}

expect_contract_compile_failure "logical EEPROM size drift" 36864u 41u
expect_contract_compile_failure "no room for a user datablock" 4096u 3900u

# The link-time guard names the region's start: XIP base plus the 1 MiB
# offset the layout test checks.
grep -q '^ASSERT(__flash_binary_end <= 0x10100000, ' "$ROOT/users/noah/lib/compat/qmk_profile_flash.ld"
grep -q 'qmk_profile_flash.ld' "$ROOT/users/noah/rules.mk"

echo "profile storage layout compile guards passed"
