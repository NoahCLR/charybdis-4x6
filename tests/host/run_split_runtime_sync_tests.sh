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

# The lighting facts frame is built from the synced fact slices (preview,
# combos, behaviour), fed by this test's fake builders. The engine's targets,
# paints and other kinds are compiled out so dispatch names only those slices.
LIGHTING_FLAGS="-DNOAH_LIGHTING_PAINT_OUTCOME=0 -DRGB_MATRIX_ENABLE -DCOMBO_ENABLE -DRGB_COMBO_FEEDBACK_ENABLE
    -DNOAH_LIGHTING_FACT_LAYER_ACTIVE=0 -DNOAH_LIGHTING_FACT_POINTING_MODE_ACTIVE=0
    -DNOAH_LIGHTING_TARGET_WHOLE_BOARD=0 -DNOAH_LIGHTING_TARGET_LEFT_HALF=0 -DNOAH_LIGHTING_TARGET_RIGHT_HALF=0
    -DNOAH_LIGHTING_TARGET_KEY_SET=0 -DNOAH_LIGHTING_TARGET_SUBJECT_KEYS=0 -DNOAH_LIGHTING_TARGET_SUBJECT_HALVES=0
    -DNOAH_LIGHTING_TARGET_SUBJECT_LAYER_KEYS=0 -DNOAH_LIGHTING_TARGET_SUBJECT_PLACE=0
    -DNOAH_LIGHTING_PAINT_CLEAR=0 -DNOAH_LIGHTING_PAINT_OFF=0 -DNOAH_LIGHTING_PAINT_COLOUR=0 -DNOAH_LIGHTING_PAINT_IDENTITY=0
    -DNOAH_LIGHTING_PAINT_MAP=0 -DNOAH_LIGHTING_PAINT_DIM=0 -DNOAH_LIGHTING_PAINT_TINT=0
    -DNOAH_LIGHTING_TIMING_SHOW_AFTER=0 -DNOAH_LIGHTING_EXTENSION_AUTOMOUSE_FADE=0 -DNOAH_LIGHTING_EXTENSION_TAP_COMMIT_FLASH=0"
LIGHTING="$ROOT/users/noah/lib/rgb/lighting"
LIGHTING_SOURCES="$LIGHTING/lighting_dispatch.c $LIGHTING/kinds/fact_layer_previewing.c $LIGHTING/kinds/fact_combos_held.c $LIGHTING/kinds/fact_behaviour_state.c $LIGHTING/kinds/fact_combo_held.c $LIGHTING/kinds/fact_behaviour.c"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DSPLIT_RUNTIME_SYNC_PUBLISH_TEST_BACKEND \
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
    $LIGHTING_FLAGS \
    "$ROOT/tests/host/split_runtime_sync_test.c" \
    "$ROOT/tests/host/split_runtime_sync_peer.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/split/runtime_sync_dirty.c" \
    "$ROOT/users/noah/lib/split/runtime_sync.c" \
    $LIGHTING_SOURCES \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -o "$BIN"

"$BIN"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DSPLIT_RUNTIME_SYNC_PUBLISH_TEST_BACKEND \
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
    $LIGHTING_FLAGS \
    "$ROOT/tests/host/split_runtime_sync_test.c" \
    "$ROOT/tests/host/split_runtime_sync_peer.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/split/runtime_sync_dirty.c" \
    "$ROOT/users/noah/lib/split/runtime_sync.c" \
    $LIGHTING_SOURCES \
    -o "$BIN_NO_TRIGGER_HALF"

"$BIN_NO_TRIGGER_HALF"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DSPLIT_RUNTIME_SYNC_PUBLISH_TEST_BACKEND \
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
