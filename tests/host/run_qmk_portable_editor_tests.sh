#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
for variant in normal features sanitized; do
    flags=""
    if [ "$variant" != normal ]; then flags="-DMAGIC_ENABLE -DNKRO_ENABLE -DAUTOCORRECT_ENABLE -DENABLE_RGB_MATRIX_BREATHING -DENABLE_RGB_MATRIX_CYCLE_ALL"; fi
    if [ "$variant" = sanitized ]; then flags="$flags -fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    cc -std=c11 -Wall -Wextra -Werror $flags -DNOAH_PORTABLE_PROFILE_ENABLE -DQMK_KEYBOARD_H='"keycode_config.h"' \
        -I"$ROOT/tests/host/include/portable_editor" -I"$ROOT/tests/host/include" -I"$ROOT" \
        -I"$QMK_ROOT/quantum/rgb_matrix/animations" \
        "$ROOT/tests/host/qmk_portable_editor_test.c" "$ROOT/users/noah/lib/compat/qmk_portable_editor.c" -o "$BUILD_DIR/test"
    "$BUILD_DIR/test" "$BUILD_DIR/options.fixture"


done
