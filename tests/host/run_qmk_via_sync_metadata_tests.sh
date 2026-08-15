#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/qmk_via_sync_metadata_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$ROOT" \
    "$ROOT/tests/host/qmk_via_sync_metadata_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_sync_metadata.c" \
    -o "$BIN"

"$BIN"
