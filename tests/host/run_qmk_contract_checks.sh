#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
QMK_ROOT="${QMK_ROOT:-$ROOT/../bastardkb-qmk}"
BUILD_DIR="$(mktemp -d)"
STUB_BIN="$BUILD_DIR/qmk_contract_stub_probe"
REAL_BIN="$BUILD_DIR/qmk_contract_real_probe"
STUB_OUT="$BUILD_DIR/stub.txt"
REAL_OUT="$BUILD_DIR/real.txt"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

if [ ! -d "$QMK_ROOT" ]; then
    echo "QMK_ROOT does not exist: $QMK_ROOT" >&2
    exit 1
fi

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -DQMK_CONTRACT_USE_STUB \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/qmk_contract_probe.c" \
    -o "$STUB_BIN"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -I"$QMK_ROOT" \
    -I"$QMK_ROOT/quantum" \
    -I"$QMK_ROOT/quantum/keymap_extras" \
    -I"$QMK_ROOT/quantum/send_string" \
    -I"$QMK_ROOT/quantum/sequencer" \
    -I"$QMK_ROOT/tmk_core/protocol" \
    -I"$QMK_ROOT/tmk_core/common" \
    -I"$QMK_ROOT/platforms" \
    "$ROOT/tests/host/qmk_contract_probe.c" \
    -o "$REAL_BIN"

"$STUB_BIN" >"$STUB_OUT"
"$REAL_BIN" >"$REAL_OUT"

if ! diff -u "$STUB_OUT" "$REAL_OUT"; then
    echo "QMK contract drift detected between tests/host/include and QMK_ROOT=$QMK_ROOT" >&2
    exit 1
fi

echo "qmk contract checks passed"
