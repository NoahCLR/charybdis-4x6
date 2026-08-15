#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/runtime_debug_test"
SMALL_TOKEN_BIN="$BUILD_DIR/runtime_debug_small_token_domain_test"

. "$ROOT/tests/host/noah_source_manifest.sh"
RUNTIME_DEBUG_SUPPORT_SOURCES="$(noah_host_runtime_debug_support_paths "$ROOT")"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

if grep -E 'next_token_id[[:space:]]*\+\+' "$ROOT/users/noah/lib/key/runtime/reducer/runtime.c" >/dev/null; then
    echo "press-token identity must use the rollover-safe allocator" >&2
    exit 1
fi

# Intentional word splitting for derived support source list.
# shellcheck disable=SC2086
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_TRACE_ENABLE \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/runtime_debug_test.c" \
    $RUNTIME_DEBUG_SUPPORT_SOURCES \
    -o "$BIN"

"$BIN"

# Rebuild the same harness with a deliberately tiny identity domain so the
# otherwise unreachable allocator-exhaustion branch is exercised directly.
# Intentional word splitting for derived support source list.
# shellcheck disable=SC2086
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_TRACE_ENABLE \
    -DKEY_RUNTIME_CORE_TOKEN_ID_MAX=3u \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/runtime_debug_test.c" \
    $RUNTIME_DEBUG_SUPPORT_SOURCES \
    -o "$SMALL_TOKEN_BIN"

"$SMALL_TOKEN_BIN"
