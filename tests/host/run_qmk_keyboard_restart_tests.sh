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

# The module twice: the USB half under its own names, the other half with
# every name it defines or calls on its side prefixed `peer_`.
PEER_NAMES="
    -Dnoah_qmk_keyboard_restart_encode=peer_restart_encode
    -Dnoah_qmk_keyboard_restart_frame_valid=peer_restart_frame_valid
    -Dnoah_qmk_keyboard_restart_request=peer_restart_request
    -Dnoah_qmk_keyboard_restart_pending=peer_restart_pending
    -Dnoah_qmk_keyboard_restart_task=peer_restart_task
    -Dnoah_qmk_keyboard_restart_reset_for_test=peer_restart_reset_for_test
    -Dsoft_reset_keyboard=peer_soft_reset_keyboard
    -Dtransaction_register_rpc=peer_transaction_register_rpc
    -Dtransaction_rpc_exec=peer_transaction_rpc_exec
    -Dwait_ms=peer_wait_ms
"

build_and_run() {
    name="$1"
    shift
    set -- -std=c11 -Wall -Wextra -Werror -pedantic "$@" \
        -DSPLIT_KEYBOARD \
        -DNOAH_QMK_KEYBOARD_RESTART_TEST \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include"
    cc "$@" -c "$ROOT/users/noah/lib/compat/qmk_keyboard_restart.c" -o "$BUILD_DIR/usb_$name.o"
    # shellcheck disable=SC2086
    cc "$@" $PEER_NAMES -c "$ROOT/users/noah/lib/compat/qmk_keyboard_restart.c" -o "$BUILD_DIR/peer_$name.o"
    cc "$@" "$ROOT/tests/host/qmk_keyboard_restart_test.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        "$BUILD_DIR/usb_$name.o" "$BUILD_DIR/peer_$name.o" \
        -o "$BUILD_DIR/qmk_keyboard_restart_test_$name"
    "$BUILD_DIR/qmk_keyboard_restart_test_$name"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer
