#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/qmk_profile_split_transport_test"
SANITIZED_BIN="$BUILD_DIR/qmk_profile_split_transport_sanitized_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

build_test() {
    output="$1"
    shift

    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        "$@" \
        -DVIA_ENABLE \
        -DSPLIT_KEYBOARD \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/qmk_profile_split_transport_test.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_authority.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_protocol_v1.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_reconciler.c" \
        "$ROOT/users/noah/lib/compat/qmk_profile_split_transport.c" \
        -o "$output"
}

build_test "$BIN"
"$BIN"

build_test "$SANITIZED_BIN" -fsanitize=address,undefined -fno-omit-frame-pointer
"$SANITIZED_BIN"
