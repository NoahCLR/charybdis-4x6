#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/via_macro_defaults_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# Compile the selected QMK checkout's native buffer dispatch, including its
# real hook short-circuit and reply, against our production via_command_kb.
# Other VIA commands are outside this probe and keep their existing tests.
python3 - "$QMK_ROOT" "$BUILD_DIR/native_via_buffer_dispatch.c" <<'PY'
from pathlib import Path
import sys

source = (Path(sys.argv[1]) / "quantum/via.c").read_text()

def braced(start):
    brace = source.index("{", start)
    depth = 0
    for end in range(brace, len(source)):
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if depth == 0:
                return brace, end + 1
    raise SystemExit("unterminated native VIA block")

start, end = braced(source.index("void raw_hid_receive("))
switch = source.index("switch (*command_id)", start)
_, switch_end = braced(switch)
prefix = source[start + 1:switch]
if "if (via_command_kb(data, length))" not in prefix or "return;" not in prefix:
    raise SystemExit("native VIA must short-circuit when via_command_kb handles a frame")
cases = []
for command in ("id_dynamic_keymap_get_buffer", "id_dynamic_keymap_set_buffer",
                "id_dynamic_keymap_macro_get_buffer", "id_dynamic_keymap_macro_set_buffer"):
    case_start = source.index(f"case {command}:", switch, switch_end)
    _, case_end = braced(case_start)
    cases.append(source[case_start:case_end])
suffix = source[switch_end:end - 1]
if "raw_hid_send(data, length);" not in suffix:
    raise SystemExit("native VIA must reply after buffer dispatch")
Path(sys.argv[2]).write_text(
    '#include "via.h"\n#include "dynamic_keymap.h"\n#include "raw_hid.h"\n'
    'bool via_command_kb(uint8_t *data, uint8_t length);\n'
    'void noah_test_native_via_buffer_dispatch(uint8_t *data, uint8_t length) {\n'
    + prefix + 'switch (*command_id) {\n' + '\n'.join(cases)
    + '\ndefault: *command_id = id_unhandled; break;\n}' + suffix + '\n}\n'
)
PY

build_and_run() {
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic "$@" \
    -DVIA_ENABLE \
    -DVIA_EEPROM_ALLOW_RESET \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/via_macro_defaults_test.c" \
    "$ROOT/users/noah/lib/macro/macro_slot_provider.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_decode_qmk.c" \
    "$ROOT/users/noah/lib/macro/via_macro_provider.c" \
    "$ROOT/users/noah/lib/compat/qmk_via_contract.c" \
    "$ROOT/users/noah/lib/macro/via_macro_defaults.c" \
    "$BUILD_DIR/native_via_buffer_dispatch.c" \
    -o "$BIN"

"$BIN"
}

build_and_run
build_and_run -fsanitize=address,undefined -fno-omit-frame-pointer
