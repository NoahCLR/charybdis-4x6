#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

if [ ! -d "$QMK_ROOT" ]; then
    echo "QMK_ROOT does not exist: $QMK_ROOT" >&2
    exit 1
fi

run_variant() {
    name="$1"
    shift
    stub_bin="$BUILD_DIR/qmk_contract_stub_probe_$name"
    real_bin="$BUILD_DIR/qmk_contract_real_probe_$name"
    stub_out="$BUILD_DIR/stub_$name.txt"
    real_out="$BUILD_DIR/real_$name.txt"

    cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
        -DQMK_CONTRACT_USE_STUB \
        "$@" \
        -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/qmk_contract_probe.c" \
        -o "$stub_bin"

    cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
        "$@" \
        -I"$QMK_ROOT" \
        -I"$QMK_ROOT/quantum" \
        -I"$QMK_ROOT/quantum/keymap_extras" \
        -I"$QMK_ROOT/quantum/send_string" \
        -I"$QMK_ROOT/quantum/sequencer" \
        -I"$QMK_ROOT/tmk_core/protocol" \
        -I"$QMK_ROOT/tmk_core/common" \
        -I"$QMK_ROOT/platforms" \
        "$ROOT/tests/host/qmk_contract_probe.c" \
        -o "$real_bin"

    "$stub_bin" >"$stub_out"
    "$real_bin" >"$real_out"

    if ! diff -u "$stub_out" "$real_out"; then
        echo "QMK contract drift ($name) between tests/host/include and QMK_ROOT=$QMK_ROOT" >&2
        exit 1
    fi
}

run_variant normal
run_variant extra_short -DEXTRA_SHORT_COMBOS

PYTHONDONTWRITEBYTECODE=1 python3 - "$QMK_ROOT" <<'PY'
from pathlib import Path
import re
import sys

qmk = Path(sys.argv[1])
quantum = (qmk / "quantum/quantum.c").read_text(encoding="utf-8")
keyboard = (qmk / "quantum/keyboard.c").read_text(encoding="utf-8")
auto_mouse = (qmk / "quantum/pointing_device/pointing_device_auto_mouse.c").read_text(encoding="utf-8")
auto_mouse_header = (qmk / "quantum/pointing_device/pointing_device_auto_mouse.h").read_text(encoding="utf-8")

def body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise SystemExit(f"unterminated function: {signature}")

pre = body(quantum, "bool pre_process_record_quantum(keyrecord_t *record)")
if pre.index("pre_process_record_kb(") > pre.index("process_combo("):
    raise SystemExit("QMK pre-process hook no longer runs before process_combo")

matrix = body(keyboard, "static bool matrix_task(void)")
if matrix.index("matrix_scan();") > matrix.index("action_exec("):
    raise SystemExit("QMK matrix_scan no longer precedes matrix event processing")

task = body(keyboard, "void keyboard_task(void)")
if task.index("matrix_task()") > task.index("quantum_task();"):
    raise SystemExit("QMK matrix task no longer precedes quantum_task")

quantum_task = body(keyboard, "void quantum_task(void)")
if "combo_task();" not in quantum_task:
    raise SystemExit("QMK quantum_task no longer runs combo_task")

elapsed_at_signature = "uint16_t auto_mouse_get_time_elapsed_at(uint16_t now)"
if not re.search(r"uint16_t\s+auto_mouse_get_time_elapsed_at\(uint16_t now\);", auto_mouse_header):
    raise SystemExit("QMK auto-mouse elapsed-at declaration is missing")

elapsed_at = body(auto_mouse, elapsed_at_signature)
if "(uint16_t)(now - auto_mouse_context.timer.active)" not in elapsed_at:
    raise SystemExit("QMK auto-mouse elapsed-at no longer uses wrap-safe caller time")

elapsed = body(auto_mouse, "uint16_t auto_mouse_get_time_elapsed(void)")
if "auto_mouse_get_time_elapsed_at(timer_read())" not in elapsed:
    raise SystemExit("QMK auto-mouse elapsed wrapper no longer delegates through elapsed-at")
PY

echo "qmk contract checks passed"
