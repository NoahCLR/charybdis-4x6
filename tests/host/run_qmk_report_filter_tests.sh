#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
case "$(uname -s)" in Darwin) link_gc="-Wl,-dead_strip" ;; *) link_gc="-Wl,--gc-sections" ;; esac
for mode in plain nkro vusb; do
    case "$mode" in plain) flags="" ;; nkro) flags="-DNKRO_ENABLE -DNKRO_SHARED_EP" ;; vusb) flags="-DPROTOCOL_VUSB" ;; esac
    cc -std=gnu11 -w -ffunction-sections -fdata-sections $flags \
        -I"$QMK_ROOT/quantum" -I"$QMK_ROOT/quantum/sequencer" \
        -I"$QMK_ROOT/tmk_core/protocol" -I"$QMK_ROOT/platforms" \
        -I"$QMK_ROOT/quantum/logging" \
        -c "$QMK_ROOT/quantum/action_util.c" -o "$BUILD_DIR/action_util.o"
    cc -std=gnu11 -Wall -Wextra -Werror $flags \
        -I"$QMK_ROOT/quantum" -I"$QMK_ROOT/tmk_core/protocol" \
        -I"$QMK_ROOT/platforms" -I"$QMK_ROOT/quantum/sequencer" \
        "$ROOT/tests/host/qmk_report_filter_test.c" "$BUILD_DIR/action_util.o" \
        $link_gc -o "$BUILD_DIR/test"
    "$BUILD_DIR/test"
done
