#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
for variant in normal sanitized; do
    flags=""
    if [ "$variant" = sanitized ]; then flags="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    cc -std=c11 -Wall -Wextra -Werror $flags -DNOAH_PORTABLE_PROFILE_ENABLE -DNOAH_PD_PROFILE_ENABLE -DQMK_KEYBOARD_H='"portable_profile_keyboard.h"' \
        -I"$ROOT/tests/host/include/portable_profile" -I"$ROOT/tests/host/include" -I"$ROOT" -I"$ROOT/users/noah" \
        "$ROOT/tests/host/qmk_portable_profile_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_portable_profile.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_settings_defaults.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_settings_runtime.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        -o "$BUILD_DIR/test"
    "$BUILD_DIR/test" "$BUILD_DIR/responses.fixture" "$BUILD_DIR/named.fixture"
    # The app reads the firmware-produced pages and finds the macro's name,
    # and with nothing stored, the keymap's layer and macro names.


done
