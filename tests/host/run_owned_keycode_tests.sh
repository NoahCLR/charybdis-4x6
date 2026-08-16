#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_require_tool rg
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/owned_keycode_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/owned_keycode_test.c" \
    "$ROOT/users/noah/lib/action/owned_keycode.c" \
    -o "$BIN"

"$BIN"

RAW_REPORT_CALLERS="$(rg -l '\b(register_code|unregister_code|register_code16|unregister_code16|add_key|del_key|register_mouse|host_consumer_send|host_system_send)[[:space:]]*\(' "$ROOT/users/noah" -g '*.c' | sed "s#^$ROOT/##" | sort)"
EXPECTED_RAW_REPORT_CALLERS='users/noah/lib/action/action_kind_dispatch.c
users/noah/lib/action/owned_keycode.c'

if [ "$RAW_REPORT_CALLERS" != "$EXPECTED_RAW_REPORT_CALLERS" ]; then
    echo "raw QMK report mutation escaped the reviewed action boundary" >&2
    printf '%s\n' "$RAW_REPORT_CALLERS" >&2
    exit 1
fi

UNSCOPED_OWNED_CALLERS="$(rg -l '\bowned_keycode_(register|unregister)[[:space:]]*\(' "$ROOT/users/noah" -g '*.c' | sed "s#^$ROOT/##" | sort)"
EXPECTED_UNSCOPED_OWNED_CALLERS='users/noah/lib/action/action_kind_dispatch.c
users/noah/lib/action/owned_keycode.c'

if [ "$UNSCOPED_OWNED_CALLERS" != "$EXPECTED_UNSCOPED_OWNED_CALLERS" ]; then
    echo "unscoped owned-keycode use escaped the reviewed literal fallback boundary" >&2
    printf '%s\n' "$UNSCOPED_OWNED_CALLERS" >&2
    exit 1
fi
