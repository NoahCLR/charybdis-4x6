#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
for variant in normal sanitized; do
    flags=""
    if [ "$variant" = sanitized ]; then flags="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter $flags \
        -DNOAH_PORTABLE_PROFILE_ENABLE -DCOMBO_ENABLE -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/participation_test.c" \
        "$ROOT/users/noah/lib/key/behavior/participation.c" \
        -o "$BUILD_DIR/test"
    "$BUILD_DIR/test"
done
