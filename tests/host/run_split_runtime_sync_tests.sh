#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/split_runtime_sync_test"
BIN_NO_TRIGGER_HALF="$BUILD_DIR/split_runtime_sync_test_no_trigger_half"
OBJ_NO_GRADIENT="$BUILD_DIR/runtime_sync_no_gradient.o"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DPOINTING_DEVICE_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DAUTOMOUSE_RGB_SYNC_STEP=10u \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/split_runtime_sync_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/split/runtime_sync_dirty.c" \
    "$ROOT/users/noah/lib/split/runtime_sync.c" \
    -o "$BIN"

"$BIN"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DPOINTING_DEVICE_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DAUTOMOUSE_RGB_SYNC_STEP=10u \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/split_runtime_sync_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/split/runtime_sync_dirty.c" \
    "$ROOT/users/noah/lib/split/runtime_sync.c" \
    -o "$BIN_NO_TRIGGER_HALF"

"$BIN_NO_TRIGGER_HALF"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DPOINTING_DEVICE_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -c "$ROOT/users/noah/lib/split/runtime_sync.c" \
    -o "$OBJ_NO_GRADIENT"

if nm -u "$OBJ_NO_GRADIENT" | grep -q 'auto_mouse_get_time_elapsed'; then
    echo "runtime_sync references auto-mouse elapsed without the RGB gradient field" >&2
    exit 1
fi
