#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/pd_mode_key_runtime_integration_test"
LEGACY_BIN="$BUILD_DIR/pd_mode_key_runtime_legacy_pinch_integration_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

compile_test() {
    bin="$1"
    shift

    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DPOINTING_DEVICE_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DNOAH_RUNTIME_TRACE_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
    -DAUTO_MOUSE_DEFAULT_LAYER=1 \
    -DCHARYBDIS_AUTO_SNIPING_LAYER=3 \
    -DDPI_MOD=0x7000u \
    -DDPI_RMOD=0x7001u \
    -DS_D_MOD=0x7002u \
    -DS_D_RMOD=0x7003u \
    -DSPLIT_TRANSACTION_IDS_USER \
    "$@" \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_integration_harness.c" \
    "$ROOT/tests/host/pd_mode_key_runtime_integration_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/action/action_dispatch.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/action/action_kind_dispatch.c" \
    "$ROOT/users/noah/lib/action/action_lifecycle.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_defaults.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_resolution_accessors.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_lookup.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_transparency.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_materialize.c" \
    "$ROOT/users/noah/lib/key/interaction/key_behavior_lookup.c" \
    "$ROOT/users/noah/lib/key/ownership/held_action.c" \
    "$ROOT/users/noah/lib/key/ownership/held_repeat.c" \
    "$ROOT/users/noah/lib/key/runtime/api.c" \
    "$ROOT/users/noah/lib/key/runtime/debug.c" \
    "$ROOT/users/noah/lib/key/runtime/origin_registry.c" \
    "$ROOT/users/noah/lib/key/runtime/preflight.c" \
    "$ROOT/users/noah/lib/key/runtime/press.c" \
    "$ROOT/users/noah/lib/key/runtime/process.c" \
    "$ROOT/users/noah/lib/key/runtime/release.c" \
    "$ROOT/users/noah/lib/key/runtime/deferred_release.c" \
    "$ROOT/users/noah/lib/key/runtime/scan.c" \
    "$ROOT/users/noah/lib/key/runtime/trace.c" \
    "$ROOT/users/noah/lib/key/runtime/transition.c" \
    "$ROOT/users/noah/lib/key/runtime/core/runtime.c" \
    "$ROOT/users/noah/lib/key/runtime/core/release_planner.c" \
    "$ROOT/users/noah/lib/key/runtime/core/trace.c" \
    "$ROOT/users/noah/lib/compat/qmk_combo_origin.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_snapshot.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_registry.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_lifecycle.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_runtime.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_key_runtime_bridge.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_state.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_pinch.c" \
    "$ROOT/users/noah/lib/pointing/policy/pointer_layer_policy.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_diag.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_shared_state.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_trace.c" \
    -o "$bin"
}

compile_test "$BIN"
"$BIN"

compile_test "$LEGACY_BIN" -DPD_MODE_KEY_RUNTIME_TEST_LEGACY_PINCH_IMPLICIT
"$LEGACY_BIN"
