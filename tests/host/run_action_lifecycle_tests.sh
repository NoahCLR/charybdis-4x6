#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/action_lifecycle_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/action_lifecycle_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/action/action_kind_dispatch.c" \
    "$ROOT/users/noah/lib/action/action_lifecycle.c" \
    -o "$BIN"

"$BIN"

# OSL() is owned by userspace, so QMK's one-shot timeout and tap-toggle apply
# to OSM() only; a build that sets them says so rather than ignoring them.
NOTICE="$(cc -std=c11 -fsyntax-only -Wno-error \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' -DONESHOT_TIMEOUT=3000 -DONESHOT_TAP_TOGGLE=2 \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    "$ROOT/users/noah/lib/action/action_kind_dispatch.c" 2>&1 || true)"
for setting in ONESHOT_TIMEOUT ONESHOT_TAP_TOGGLE; do
    if ! printf '%s\n' "$NOTICE" | grep -q "$setting applies to OSM() only"; then
        echo "a build setting $setting did not say that it applies to OSM() only" >&2
        exit 1
    fi
done
QUIET="$(cc -std=c11 -fsyntax-only -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    "$ROOT/users/noah/lib/action/action_kind_dispatch.c" 2>&1 || true)"
if printf '%s\n' "$QUIET" | grep -q "applies to OSM() only"; then
    echo "the one-shot notice appeared without either setting" >&2
    exit 1
fi
