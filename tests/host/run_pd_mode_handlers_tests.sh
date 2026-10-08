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

python3 - "$ROOT" "$BUILD_DIR/pd_mode_engine_fixture.h" <<'PYFIX'
import json, sys
from pathlib import Path
fixture=json.loads(Path(sys.argv[1], 'tests/fixtures/pd_mode_domain_v1.json').read_text())
Path(sys.argv[2]).write_text('static const unsigned char pd_engine_fixture[] = {' + ','.join(str(b) for b in bytes.fromhex(fixture['hex'])) + '};\n')
PYFIX

cc -DNOAH_PD_PROFILE_ENABLE -I"$BUILD_DIR" -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DPOINTING_DEVICE_ENABLE \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config_no_automouse.h" \
    "$ROOT/tests/host/pd_mode_handlers_test.c" \
    "$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah/pd_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_pd_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_configured.c" \
    "$ROOT/users/noah/lib/pointing/modes/pd_mode_dragscroll.c" \
    "$ROOT/users/noah/lib/state/modifiers/keyboard_mod_state.c" \
    -o "$BIN"

"$BIN"

expect_config_compile_failure() {
    label="$1"
    shift

    if cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -pedantic \
        -DNOAH_PD_PROFILE_ENABLE \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DPOINTING_DEVICE_ENABLE \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config_no_automouse.h" \
        "$@" \
        -c "$ROOT/users/noah/lib/pointing/modes/pd_mode_configured.c" \
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

echo "pd_mode_handlers config guards passed"
