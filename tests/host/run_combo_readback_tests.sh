#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -DVIA_ENABLE -DCOMBO_ENABLE -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' "$@" \
        -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        "$ROOT/tests/host/qmk_combo_readback_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_combo_readback.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        -o "$BUILD_DIR/test_$name"
    if [ "$name" = normal ] || [ "$name" = sanitized ]; then
        "$BUILD_DIR/test_$name" "$ROOT/tests/fixtures/combo_readback_eight_v1.fixture"
    else
        "$BUILD_DIR/test_$name"
    fi
}
build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer
build_and_run callbacks -DCOMBO_TERM_PER_COMBO -DCOMBO_MUST_HOLD_PER_COMBO -DCOMBO_MUST_TAP_PER_COMBO -DCOMBO_MUST_PRESS_IN_ORDER_PER_COMBO
build_and_run fixed_layer_no_timer -DCOMBO_ONLY_FROM_LAYER=1 -DCOMBO_NO_TIMER -DCOMBO_TERM_PER_COMBO -DCOMBO_MUST_HOLD_PER_COMBO -DCOMBO_MUST_TAP_PER_COMBO -DCOMBO_MUST_PRESS_IN_ORDER_PER_COMBO
