#!/bin/sh

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

run_variant() {
    name="$1"
    shift
    bin="$BUILD_DIR/qmk_combo_origin_test_$name"

    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -DCOMBO_ENABLE \
        -DCOMBO_ORIGIN_TEST_INSTRUMENTATION \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        "$@" \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        "$ROOT/tests/host/qmk_combo_origin_test.c" \
        "$ROOT/users/noah/lib/key/runtime/slot/origin_registry.c" \
        "$ROOT/users/noah/lib/compat/qmk_combo_origin.c" \
        "$ROOT/users/noah/lib/key/behavior/participation.c" \
        -o "$bin"

    "$bin"
}

run_variant normal
run_variant extra_short -DEXTRA_SHORT_COMBOS
run_variant output_permissions -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE
run_variant output_permissions_sanitized -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE -fsanitize=address,undefined -fno-omit-frame-pointer

# Timerless combos can remain legally pending until release. Compile that
# branch to ensure its deliberately unbounded deadline path stays warning-free.
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DCOMBO_ENABLE \
    -DCOMBO_NO_TIMER \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config.h" \
    -c "$ROOT/users/noah/lib/compat/qmk_combo_origin.c" \
    -o "$BUILD_DIR/qmk_combo_origin_no_timer.o"
