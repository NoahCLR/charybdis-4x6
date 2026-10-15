#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_batch.c|users/noah/lib/rgb/lighting/lighting_batch_arena.c|users/noah/lib/rgb/lighting/lighting_owned_batch.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
for matrix in '4 6' '10 6' '8 8'; do
    set -- $matrix
    for variant in normal sanitized; do
        flags=""
        [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
        # shellcheck disable=SC2086
        cc -std=c11 -Wall -Wextra -Werror -pedantic $flags \
          -DQMK_KEYBOARD_H='"qmk_stub.h"' -DMATRIX_ROWS="$1" -DMATRIX_COLS="$2" \
          -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
          "$ROOT/tests/host/lighting_owned_batch_test.c" $SOURCES -o "$BUILD_DIR/$variant"
        "$BUILD_DIR/$variant"
    done
done
