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
    case "$src" in users/noah/lib/rgb/lighting/kinds/event_*.c|users/noah/lib/rgb/lighting/lighting_occurrence.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
for matrix in '4 6' '10 6' '8 8'; do
    set -- $matrix
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer \
      -DQMK_KEYBOARD_H='"qmk_stub.h"' -DRGB_MATRIX_ENABLE -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DMATRIX_ROWS="$1" -DMATRIX_COLS="$2" \
      -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/lighting_occurrence_geometry_test.c" $SOURCES \
      "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" -o "$BUILD_DIR/geometry"
    "$BUILD_DIR/geometry"
done
