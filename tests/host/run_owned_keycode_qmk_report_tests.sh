#!/bin/sh
# A custom key's shortcut through the real owned output and the selected QMK's
# own report code (owned_keycode_qmk_report_test.c): QMK's action.c,
# action_util.c, host.c, mousekey.c and pointing_device.c, compiled with QMK's
# own headers and the features the firmware builds with (extrakey, mouse keys,
# a pointing device; no NKRO), and linked with only what the test reaches.
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM

FEATURES="-DEXTRAKEY_ENABLE -DMOUSEKEY_ENABLE -DMOUSE_ENABLE -DPOINTING_DEVICE_ENABLE -DPOINTING_DEVICE_DRIVER_custom -DPOINTING_DEVICE_DRIVER_NAME=custom -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNO_DEBUG -DNO_PRINT"
# QMK's test platform supplies platform_deps.h and wait_ms(); it is searched
# after platforms/, so platforms/wait.h finds its _wait.h.
INCLUDES="-I$QMK_ROOT/quantum -I$QMK_ROOT/quantum/logging -I$QMK_ROOT/quantum/keymap_extras -I$QMK_ROOT/quantum/pointing_device -I$QMK_ROOT/quantum/sequencer -I$QMK_ROOT/tmk_core/protocol -I$QMK_ROOT/platforms -I$QMK_ROOT/drivers -idirafter $QMK_ROOT/platforms/test"
# Every file, QMK's and ours, sees the firmware's own configuration, as the
# build gives it, so both sides agree on QMK's report layouts.
CONFIG="-I$ROOT -include $ROOT/tests/host/include/noah_compile_config.h"
case "$(uname -s)" in Darwin) link_gc="-Wl,-dead_strip" ;; *) link_gc="-Wl,--gc-sections" ;; esac

objects=""
for source in quantum/action.c quantum/action_util.c quantum/logging/debug.c tmk_core/protocol/host.c tmk_core/protocol/report.c quantum/mousekey.c quantum/pointing_device/pointing_device.c; do
    object="$BUILD_DIR/$(basename "$source" .c).o"
    # shellcheck disable=SC2086
    cc -std=gnu11 -w -ffunction-sections -fdata-sections $FEATURES $CONFIG $INCLUDES -c "$QMK_ROOT/$source" -o "$object"
    objects="$objects $object"
done

# shellcheck disable=SC2086
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    $FEATURES -DQMK_KEYBOARD_H='"quantum.h"' \
    $CONFIG \
    -I"$ROOT" -I"$ROOT/users/noah" $INCLUDES \
    "$ROOT/tests/host/owned_keycode_qmk_report_test.c" \
    "$ROOT/users/noah/lib/action/owned_keycode.c" \
    "$ROOT/users/noah/lib/state/ownership/keyboard_mod_ownership.c" \
    "$ROOT/users/noah/lib/state/shared/runtime_shared_state.c" \
    $objects $link_gc -o "$BUILD_DIR/test"
"$BUILD_DIR/test"
