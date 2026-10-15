#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCE=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_batch.c) SOURCE="$ROOT/$src" ;; esac
done
[ -n "$SOURCE" ]
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' \
      -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/lighting_batch_test.c" "$SOURCE" -o "$BUILD_DIR/$variant"
    "$BUILD_DIR/$variant"
done

# Scratch has no implicit geometry cap: a larger fact configuration enlarges
# the union and needs its own linked policy acceptance, rather than overrunning.
for matrix in '4 6' '10 6' '8 8'; do
    set -- $matrix
    for combos in 1 4 64; do
        cc -std=c11 -Wall -Wextra -Werror -pedantic -DQMK_KEYBOARD_H='"qmk_stub.h"' \
          -DRGB_MATRIX_ENABLE -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DMATRIX_ROWS="$1" -DMATRIX_COLS="$2" -DCOMBO_BUFFER_LENGTH="$combos" \
          -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
          "$ROOT/tests/host/lighting_workspace_layout_test.c" -o "$BUILD_DIR/layout"
        "$BUILD_DIR/layout"
        if [ "$combos" = 4 ]; then
            cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer \
              -DQMK_KEYBOARD_H='"qmk_stub.h"' -DMATRIX_ROWS="$1" -DMATRIX_COLS="$2" \
              -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
              "$ROOT/tests/host/lighting_batch_test.c" "$SOURCE" -o "$BUILD_DIR/geometry"
            "$BUILD_DIR/geometry"
        fi
    done
done
"${ARM_CC:-arm-none-eabi-gcc}" -std=c11 -Wall -Wextra -Werror -pedantic -mcpu=cortex-m0plus -mthumb \
  -DQMK_KEYBOARD_H='"qmk_stub.h"' -DRGB_MATRIX_ENABLE -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DCOMBO_BUFFER_LENGTH=4 \
  -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
  -c "$ROOT/tests/host/lighting_workspace_layout_test.c" -o "$BUILD_DIR/layout_arm.o"
