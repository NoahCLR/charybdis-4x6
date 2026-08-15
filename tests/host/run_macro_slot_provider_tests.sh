#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/macro_slot_provider_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    "$ROOT/tests/host/macro_slot_provider_test.c" \
    "$ROOT/users/noah/lib/macro/macro_slot_provider.c" \
    -o "$BIN"

"$BIN"
