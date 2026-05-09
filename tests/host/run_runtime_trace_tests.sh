#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/runtime_trace_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_TRACE_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DSPLIT_TRANSACTION_IDS_USER \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/runtime_trace_test.c" \
    "$ROOT/users/noah/lib/key/runtime/trace.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/origin_registry.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_snapshot.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_registry.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_lifecycle.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_state.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_dragscroll.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_pinch.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_volume.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_brightness.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_zoom.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_arrow.c" \
    "$ROOT/users/noah/lib/state/ownership/layer_ownership.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    "$ROOT/users/noah/lib/state/shared/runtime_shared_state.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_trace.c" \
    "$ROOT/users/noah/lib/key/runtime/trace/core_trace.c" \
    "$ROOT/users/noah/lib/split/runtime_sync_dirty.c" \
    "$ROOT/users/noah/lib/split/runtime_sync.c" \
    "$ROOT/tests/host/key_runtime_core_state_unit_stub.c" \
    -o "$BIN"

"$BIN"
