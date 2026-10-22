#!/bin/sh

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

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
    mutation="$2"
    shift 2
    cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter "$@" \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DNOAH_LIVE_PROFILE_OWNER_ENABLE \
        $mutation \
        -DNOAH_PORTABLE_PROFILE_ENABLE \
        -DCOMBO_ENABLE \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        "$ROOT/tests/host/qmk_profile_edit_channel_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_profile_edit_channel.c" \
        -o "$BUILD_DIR/qmk_profile_edit_channel_test_$name"
    "$BUILD_DIR/qmk_profile_edit_channel_test_$name"
}

build_and_run normal -DNOAH_LIVE_PROFILE_MUTATION_ENABLE
build_and_run sanitized -DNOAH_LIVE_PROFILE_MUTATION_ENABLE -fsanitize=address,undefined -fno-omit-frame-pointer
# An owner that cannot edit (NOAH_LIVE_PROFILE_MUTATION=no) still restarts.
build_and_run read_only ""
