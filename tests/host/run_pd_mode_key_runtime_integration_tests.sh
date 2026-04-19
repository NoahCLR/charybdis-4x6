#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/pd_mode_key_runtime_integration_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DPOINTING_DEVICE_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DAUTO_MOUSE_DEFAULT_LAYER=1 \
    -DCHARYBDIS_AUTO_SNIPING_LAYER=3 \
    -DDPI_MOD=0x7000u \
    -DDPI_RMOD=0x7001u \
    -DS_D_MOD=0x7002u \
    -DS_D_RMOD=0x7003u \
    -DSPLIT_TRANSACTION_IDS_USER \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_integration_harness.c" \
    "$ROOT/tests/host/pd_mode_key_runtime_integration_test.c" \
    "$ROOT/tests/host/runtime_v2_observer_stub.c" \
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
    "$ROOT/users/noah/lib/key/ownership/held_action.c" \
    "$ROOT/users/noah/lib/key/ownership/held_repeat.c" \
    "$ROOT/users/noah/lib/key/interaction/key_behavior_lookup.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_admission.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_release_reduce.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_result.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_scan_reduce.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_index.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_trace.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_preflight.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_press.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_process.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_release.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_scan.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_transition.c" \
    "$ROOT/users/noah/lib/key/interaction/multi_tap_engine.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_snapshot.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_registry.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_lifecycle.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_runtime.c" \
    "$ROOT/users/noah/lib/pointing/runtime/pd_mode_state.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_pinch.c" \
    "$ROOT/users/noah/lib/pointing/policy/pointer_layer_policy.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_debug.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_shared_state.c" \
    -o "$BIN"

"$BIN"
