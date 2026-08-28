#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

build_and_run() {
    name="$1"
    shift

    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        "$@" \
        "$ROOT/tests/host/qmk_physical_half_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_physical_half.c" \
        -o "$BUILD_DIR/$name"
    "$BUILD_DIR/$name"
}

build_and_run unprovisioned
build_and_run left -DNOAH_PHYSICAL_HALF_LEFT
build_and_run right -DNOAH_PHYSICAL_HALF_RIGHT

if cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_PHYSICAL_HALF_LEFT \
    -DNOAH_PHYSICAL_HALF_RIGHT \
    -include "$ROOT/tests/host/include/noah_compile_config.h" \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -c "$ROOT/users/noah/lib/compat/qmk_physical_half.c" \
    -o "$BUILD_DIR/conflicting.o" 2>/dev/null
then
    echo "conflicting physical-half provisioning unexpectedly compiled" >&2
    exit 1
fi
