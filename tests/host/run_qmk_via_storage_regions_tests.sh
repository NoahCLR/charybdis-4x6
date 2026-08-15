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
    cc -std=c11 -Wall -Wextra -Werror -pedantic \
        -DVIA_ENABLE \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$ROOT" \
        -I"$ROOT/tests/host/include" \
        "$@" \
        "$ROOT/tests/host/qmk_via_storage_regions_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_via_storage_regions.c" \
        -o "$BUILD_DIR/$name"
    "$BUILD_DIR/$name"
}

build_and_run normal
build_and_run encoder -DENCODER_MAP_ENABLE -DNUM_ENCODERS=2
