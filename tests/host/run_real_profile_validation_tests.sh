#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/real_profile_validation_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DCONSOLE_ENABLE \
    -DCOMBO_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DDPI_MOD=0x5201u \
    -DDPI_RMOD=0x5202u \
    -DS_D_MOD=0x5203u \
    -DS_D_RMOD=0x5204u \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DQMK_KEYBOARD_H='"noah_real_profile_keyboard.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config.h" \
    "$ROOT/tests/host/real_profile_validation_test.c" \
    "$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c" \
    "$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/key/interaction/key_behavior_lookup.c" \
    "$ROOT/users/noah/lib/key/interaction/keymap_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    -o "$BIN"

"$BIN"
