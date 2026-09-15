#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' -DVIA_ENABLE -DSPLIT_TRANSACTION_IDS_USER \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/qmk_via_logical_profile_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_logical_profile.c" \
    -o "$BUILD_DIR/qmk_via_logical_profile_test"
"$BUILD_DIR/qmk_via_logical_profile_test"
