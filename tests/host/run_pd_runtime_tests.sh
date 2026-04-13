#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/pd_runtime_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DPOINTING_DEVICE_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DCHARYBDIS_AUTO_SNIPING_ENABLE \
    -DCHARYBDIS_AUTO_SNIPING_LAYER=2 \
    -DAUTO_MOUSE_DEFAULT_LAYER=4 \
    -DNOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE \
    -DNOAH_POINTING_IDLE_NOISE_SUPPRESSION_IDLE_MS=1000 \
    -DNOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX=2 \
    -DDPI_MOD=0x5201u \
    -DDPI_RMOD=0x5202u \
    -DS_D_MOD=0x5203u \
    -DS_D_RMOD=0x5204u \
    -DPD_MODE_VOLUME_DPI=1400 \
    -DPD_MODE_BRIGHTNESS_DPI=1500 \
    -DPD_MODE_ZOOM_DPI=1600 \
    -DPD_MODE_ARROW_DPI=1700 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/pd_runtime_test.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_runtime.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_snapshot.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_registry.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_lifecycle.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_state.c" \
    "$ROOT/users/noah/lib/pointing/policy/pointer_layer_policy.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_pinch.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_shared_state.c" \
    -o "$BIN"

"$BIN"
