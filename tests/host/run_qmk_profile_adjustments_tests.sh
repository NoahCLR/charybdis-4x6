#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
for trigger in release press; do
    flags=""
    [ "$trigger" != press ] || flags="-DRGB_TRIGGER_ON_KEYDOWN"
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer $flags \
        -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE -DPOINTING_DEVICE_ENABLE \
        -DQMK_KEYBOARD_H='"profile_adjustments_keyboard.h"' \
        -I"$ROOT/tests/host/include" -I"$ROOT" -I"$ROOT/users/noah" \
        "$ROOT/tests/host/qmk_profile_adjustments_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_profile_adjustments.c" -o "$BUILD_DIR/test"
    "$BUILD_DIR/test"
done
