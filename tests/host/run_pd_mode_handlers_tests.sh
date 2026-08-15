#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/pd_mode_handlers_test"
CONFIG_PROBE_OBJECT="$BUILD_DIR/pd_mode_handler_config_probe.o"
CONFIG_PROBE_LOG="$BUILD_DIR/pd_mode_handler_config_probe.log"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DPOINTING_DEVICE_ENABLE \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config_no_automouse.h" \
    "$ROOT/tests/host/pd_mode_handlers_test.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_dragscroll.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_volume.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_brightness.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_zoom.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_arrow.c" \
    "$ROOT/users/noah/lib/state/modifiers/keyboard_mod_state.c" \
    -o "$BIN"

"$BIN"

expect_config_compile_failure() {
    label="$1"
    shift

    if cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -pedantic \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DPOINTING_DEVICE_ENABLE \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config_no_automouse.h" \
        "$@" \
        -c "$ROOT/users/noah/lib/pointing/modes/pd_mode_volume.c" \
        -o "$CONFIG_PROBE_OBJECT" >"$CONFIG_PROBE_LOG" 2>&1; then
        echo "expected pd-mode config compile failure: $label" >&2
        exit 1
    fi
}

expect_config_compile_failure "zero tap budget" -DNOAH_PD_MODE_MAX_TAPS_PER_TICK=0
expect_config_compile_failure "tap budget exceeds uint8_t" -DNOAH_PD_MODE_MAX_TAPS_PER_TICK=256
expect_config_compile_failure "zero backlog cap" -DNOAH_PD_MODE_MAX_BACKLOG_TAPS=0
expect_config_compile_failure "backlog cap exceeds diagnostics" -DNOAH_PD_MODE_MAX_BACKLOG_TAPS=65536
expect_config_compile_failure "tap budget exceeds backlog cap" -DNOAH_PD_MODE_MAX_TAPS_PER_TICK=33
expect_config_compile_failure "zero axis threshold" -DVOLUME_THRESHOLD=0
expect_config_compile_failure "threshold and backlog exceed accumulator" -DVOLUME_THRESHOLD=65535 -DNOAH_PD_MODE_MAX_BACKLOG_TAPS=65535

echo "pd_mode_handlers config guards passed"
