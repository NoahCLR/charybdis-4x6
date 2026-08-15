#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/pending_release_queue_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

if grep -E 'next_pending_release_sequence|pending[^;]*\.sequence|pending->sequence' "$ROOT/users/noah/lib/key/runtime/queue/pending_release_queue.c" "$ROOT/users/noah/lib/key/runtime/reducer/runtime.h" >/dev/null; then
    echo "pending-release FIFO order must not depend on a wrapping sequence" >&2
    exit 1
fi

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_HOST_TEST_ENV \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/pending_release_queue_test.c" \
    "$ROOT/users/noah/lib/key/runtime/queue/pending_release_queue.c" \
    -o "$BIN"

"$BIN"
