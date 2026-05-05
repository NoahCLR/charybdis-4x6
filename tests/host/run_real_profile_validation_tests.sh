#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

run_validation_variant() {
    name="$1"
    config_header="$2"
    bin="$BUILD_DIR/real_profile_validation_${name}_test"

    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -DCONSOLE_ENABLE \
        -DCOMBO_ENABLE \
        -DSPLIT_KEYBOARD \
        -DPOINTING_DEVICE_ENABLE \
        -DRGB_MATRIX_ENABLE \
        -DRGB_MATRIX_WS2812 \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DQMK_KEYBOARD_H='"noah_real_profile_keyboard.h"' \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/$config_header" \
        "$ROOT/tests/host/real_profile_validation_test.c" \
        "$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c" \
        "$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c" \
        "$ROOT/users/noah/lib/action/action_kind.c" \
        "$ROOT/users/noah/lib/key/behavior/key_behavior_lookup.c" \
        "$ROOT/users/noah/lib/key/behavior/keymap_validation.c" \
        "$ROOT/users/noah/lib/macro/macro_dispatch.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_encode.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_keycodes.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_parse.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_run.c" \
        "$ROOT/users/noah/lib/macro/macro_slot_provider.c" \
        "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
        "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
        -o "$bin"

    "$bin"
}

run_validation_variant default tests/host/include/noah_compile_config.h
run_validation_variant no_rgb_feedback tests/host/include/noah_compile_config_no_rgb_feedback.h
