#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -pedantic "$@" \
        -DSPLIT_KEYBOARD \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/qmk_profile_flash_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_durable_io.c" \
        "$ROOT/users/noah/lib/compat/qmk_profile_flash.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_flash_store.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_store.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_domain_registry.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        -o "$BUILD_DIR/qmk_profile_flash_test_$name"
    "$BUILD_DIR/qmk_profile_flash_test_$name"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer
