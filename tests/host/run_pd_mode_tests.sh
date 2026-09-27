#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/pd_mode_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# Registry rows preserve native identities without motion callbacks.
if [ "$(uname -s)" = Darwin ]; then
    LINK_DEAD_SECTIONS=-Wl,-dead_strip
else
    LINK_DEAD_SECTIONS=-Wl,--gc-sections
fi
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DPOINTING_DEVICE_ENABLE \
    -DNOAH_PD_PROFILE_ENABLE \
    -ffunction-sections -fdata-sections "$LINK_DEAD_SECTIONS" \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/pd_mode_profile_registry_test.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_registry.c" \
    -o "$BIN"

"$BIN"
