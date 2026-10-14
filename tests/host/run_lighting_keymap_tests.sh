#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
for matrix in '10 6' '8 8' '4 6'; do
    set -- $matrix
    for variant in normal sanitized; do
        flags=""
        if [ "$variant" = sanitized ]; then flags="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
        # shellcheck disable=SC2086
        cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic $flags \
            -DQMK_KEYBOARD_H='"qmk_stub.h"' -DMATRIX_ROWS="$1" -DMATRIX_COLS="$2" \
            -DRGB_MATRIX_ENABLE -DRGB_MATRIX_LED_COUNT=58 -DNOAH_PORTABLE_PROFILE_ENABLE \
            -DCOMBO_ENABLE -DNOAH_LIGHTING_KEYMAP_TEST_BACKEND \
            -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
            "$ROOT/tests/host/lighting_keymap_test.c" "$ROOT/users/noah/lib/rgb/lighting/lighting_keymap.c" -o "$BUILD_DIR/test"
        "$BUILD_DIR/test"
    done
done
