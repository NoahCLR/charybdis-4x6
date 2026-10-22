#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/runtime_init_order_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# The image without a profile owner, then the one with it.
for variant in no_owner owner; do
    flags=""
    if [ "$variant" = owner ]; then flags="-DNOAH_LIVE_PROFILE_OWNER_ENABLE"; fi
    # Intentional word splitting for the variant's flag.
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic $flags \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DEECONFIG_USER_DATA_SIZE=0 \
        -DSPLIT_TRANSACTION_IDS_USER \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/runtime_init_order_test.c" \
        "$ROOT/users/noah/runtime_init.c" \
        "$ROOT/users/noah/lib/key/runtime/slot/origin_registry.c" \
        -o "$BIN"
    "$BIN"
done
