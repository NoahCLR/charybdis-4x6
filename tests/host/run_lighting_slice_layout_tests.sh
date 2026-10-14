#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
. "$ROOT/tests/host/noah_source_manifest.sh"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in
        */fact_layer_status.c|*/fact_behaviour_phase.c|*/fact_combo_layer.c) SOURCES="$SOURCES $ROOT/$src";;
    esac
done
for matrix in '4 6' '10 6' '8 8'; do
    set -- $matrix
    for combos in 1 4 8; do
        # shellcheck disable=SC2086
        cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
          -fsanitize=address,undefined -fno-omit-frame-pointer \
          -DQMK_KEYBOARD_H='"qmk_stub.h"' -DMATRIX_ROWS="$1" -DMATRIX_COLS="$2" -DCOMBO_BUFFER_LENGTH="$combos" \
          -DRGB_MATRIX_ENABLE -DNOAH_LIGHTING_FACT_LAYER_STATUS=1 -DNOAH_LIGHTING_FACT_BEHAVIOUR_PHASE=1 -DNOAH_LIGHTING_FACT_COMBO_LAYER=1 \
          -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
          "$ROOT/tests/host/lighting_slice_layout_test.c" $SOURCES -o "$BUILD_DIR/probe"
        "$BUILD_DIR/probe"
    done
done
printf 'lighting slice layouts: 9 nondefault/default sanitized configurations passed\n'
