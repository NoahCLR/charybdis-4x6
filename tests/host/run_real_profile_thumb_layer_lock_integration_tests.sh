#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/real_profile_thumb_layer_lock_integration_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DNOAH_HOST_TESTS \
    -DCONSOLE_ENABLE \
    -DCOMBO_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DDPI_MOD=0x5201u \
    -DDPI_RMOD=0x5202u \
    -DS_D_MOD=0x5203u \
    -DS_D_RMOD=0x5204u \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DQMK_KEYBOARD_H='"noah_real_profile_keyboard.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config.h" \
    "$ROOT/tests/host/key_runtime_integration_harness.c" \
    "$ROOT/tests/host/real_profile_thumb_layer_lock_integration_test.c" \
    "$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/action/action_kind_dispatch.c" \
    "$ROOT/users/noah/lib/action/action_dispatch.c" \
    "$ROOT/users/noah/lib/action/action_lifecycle.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_defaults.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_resolution_accessors.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_lookup.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_transparency.c" \
    "$ROOT/users/noah/lib/key/interaction/handled_key_materialize.c" \
    "$ROOT/users/noah/lib/key/interaction/key_behavior_lookup.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_trace.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_process.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_admission.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_release_reduce.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_result.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_scan_reduce.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot_step.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_index.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_preflight.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_press.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_release.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_scan.c" \
    "$ROOT/users/noah/lib/key/runtime/slot/key_runtime_slot.c" \
    "$ROOT/users/noah/lib/key/runtime/key_runtime_transition.c" \
    "$ROOT/users/noah/lib/key/interaction/multi_tap_engine.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_debug.c" \
    "$ROOT/users/noah/lib/state/ownership/layer_ownership.c" \
    "$ROOT/users/noah/lib/state/runtime/runtime_shared_state.c" \
    -o "$BIN"

"$BIN"
