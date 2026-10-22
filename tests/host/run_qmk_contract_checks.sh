#!/bin/sh

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

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

# QMK's auto-mouse flips a toggle of its own on some layer keys; the runtime
# takes each flip back through compat/qmk_auto_mouse_contract.h. Run QMK's own
# process_auto_mouse against that contract so the two cannot drift. QMK's file
# is third-party code compiled on its own with warnings off (GCC warns where
# clang does not); the contract test itself stays -Werror.
auto_mouse_cc() {
    cc -std=gnu11 \
        -DPOINTING_DEVICE_ENABLE \
        -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
        -DAUTO_MOUSE_DELAY=200 \
        -DQMK_KEYBOARD_H='"action.h"' \
        -I"$ROOT" \
        -I"$QMK_ROOT" \
        -I"$QMK_ROOT/quantum" \
        -I"$QMK_ROOT/quantum/keymap_extras" \
        -I"$QMK_ROOT/quantum/send_string" \
        -I"$QMK_ROOT/quantum/sequencer" \
        -I"$QMK_ROOT/quantum/pointing_device" \
        -I"$QMK_ROOT/quantum/logging" \
        -I"$QMK_ROOT/tmk_core/protocol" \
        -I"$QMK_ROOT/platforms" \
        "$@"
}
auto_mouse_cc -w -c "$QMK_ROOT/quantum/pointing_device/pointing_device_auto_mouse.c" \
    -o "$BUILD_DIR/qmk_pointing_device_auto_mouse.o"
auto_mouse_cc -Wall -Wextra -Werror -Wno-unused-parameter \
    "$ROOT/tests/host/qmk_auto_mouse_toggle_contract_test.c" \
    "$BUILD_DIR/qmk_pointing_device_auto_mouse.o" \
    -o "$BUILD_DIR/qmk_auto_mouse_toggle_contract_test"
"$BUILD_DIR/qmk_auto_mouse_toggle_contract_test"

PYTHONDONTWRITEBYTECODE=1 python3 - "$QMK_ROOT" "$ROOT" <<'PY'
from pathlib import Path
import re
import sys

qmk = Path(sys.argv[1])
quantum = (qmk / "quantum/quantum.c").read_text(encoding="utf-8")
keyboard = (qmk / "quantum/keyboard.c").read_text(encoding="utf-8")
auto_mouse = (qmk / "quantum/pointing_device/pointing_device_auto_mouse.c").read_text(encoding="utf-8")
auto_mouse_header = (qmk / "quantum/pointing_device/pointing_device_auto_mouse.h").read_text(encoding="utf-8")
usb_descriptor_header = (qmk / "tmk_core/protocol/usb_descriptor.h").read_text(encoding="utf-8")
action_tapping_header = (qmk / "quantum/action_tapping.h").read_text(encoding="utf-8")
tapping_contract = (Path(sys.argv[2]) / "users/noah/lib/compat/qmk_tapping_contract.h").read_text(encoding="utf-8")

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

action = (qmk / "quantum/action.c").read_text()
if "is_tap_record_user(record, is_tap_action(action), is_tap_keycode_user(get_record_keycode(record, false), is_tap_action(action)))" not in body(action, "bool is_tap_record(keyrecord_t *record)"):
    raise SystemExit("QMK must offer authored dual-role keys, with their record, to the userspace tapping classifier")
# Its default keeps the keycode hook's answer for builds that define only that.
if "return default_tap;" not in body(action, "__attribute__((weak)) bool is_tap_record_user(keyrecord_t *record, bool native_tap, bool default_tap)"):
    raise SystemExit("QMK's record tapping hook must default to the keycode hook's answer")
for source, signature in [("quantum/action_tapping.c", "bool tapping_key_event_pending("),
                          ("quantum/process_keycode/process_combo.c", "bool combo_key_event_pending(")]:
    if signature not in (qmk / source).read_text():
        raise SystemExit(f"QMK gesture queue contract missing: {signature}")
record = body(action, "void process_record(keyrecord_t *record)")
if "process_record_admit_user(record)" not in record or record.index("process_record_admit_user(record)") > record.index("process_record_quantum(record)"):
    raise SystemExit("QMK must offer every record to process_record_admit_user before any quantum feature")

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

if not re.search(r"^\s*#\s*define\s+RAW_EPSIZE\s+32\s*$", usb_descriptor_header, re.MULTILINE):
    raise SystemExit("QMK Raw HID endpoint size no longer matches Profile Wire's 32-byte report contract")

# The keyboard name (D-F25): the fork serves a runtime product string, the
# name is read from EEPROM in keyboard_pre_init before USB starts, and the half
# on USB can restart the driver to be enumerated again.
if not re.search(r"#ifdef USB_PRODUCT_STRING_USER\s*(//[^\n]*\n\s*)*const USB_Descriptor_String_t\s*\*\s*usb_product_string_user\(void\);", usb_descriptor_header):
    raise SystemExit("QMK must declare usb_product_string_user() under USB_PRODUCT_STRING_USER (BK runtime product string hook)")
descriptors = body((qmk / "tmk_core/protocol/usb_descriptor.c").read_text(encoding="utf-8"), "uint16_t get_usb_descriptor(")
product = descriptors[descriptors.index("case 0x02:"):]
if "usb_product_string_user()" not in product or product.index("usb_product_string_user()") > product.index("&ProductString"):
    raise SystemExit("QMK must offer the product string descriptor to usb_product_string_user() before PRODUCT")
setup = body(keyboard, "void keyboard_setup(void)")
if setup.index("eeprom_driver_init();") > setup.index("keyboard_pre_init_quantum();"):
    raise SystemExit("QMK no longer initializes EEPROM before keyboard_pre_init")
main = body((qmk / "quantum/main.c").read_text(encoding="utf-8"), "int main(void) {")
if "keyboard_setup();" not in main or "protocol_pre_init();" not in main or main.index("keyboard_setup();") > main.index("protocol_pre_init();"):
    raise SystemExit("QMK no longer runs keyboard_setup before protocol_pre_init starts USB")
if "init_usb_driver(" not in body((qmk / "tmk_core/protocol/chibios/chibios.c").read_text(encoding="utf-8"), "void protocol_pre_init(void)"):
    raise SystemExit("QMK's ChibiOS protocol_pre_init no longer starts USB")
if not re.search(r"void\s+restart_usb_driver\(USBDriver \*usbp\);", (qmk / "tmk_core/protocol/chibios/usb_main.h").read_text(encoding="utf-8")):
    raise SystemExit("QMK's ChibiOS USB no longer offers restart_usb_driver")

# The profile owns runtime lookup through BK's explicitly replaceable hook.
keymap_common = (qmk / "quantum/keymap_common.c").read_text(encoding="utf-8")
if not re.search(r"__attribute__\(\(weak\)\)\s+uint16_t\s+keymap_key_to_keycode\(", keymap_common):
    raise SystemExit("QMK keymap_key_to_keycode must remain weak for profile lookup")

qmk_toggle = re.search(r"^\s*#\s*define\s+TAPPING_TOGGLE\s+(\d+)\s*$", action_tapping_header, re.MULTILINE)
our_toggle = re.search(r"^\s*#\s*define\s+NOAH_QMK_TAPPING_TOGGLE\s+(\d+)\s*$", tapping_contract, re.MULTILINE)
if not (qmk_toggle and our_toggle and qmk_toggle.group(1) == our_toggle.group(1)):
    raise SystemExit("QMK's TAPPING_TOGGLE default no longer matches compat/qmk_tapping_contract.h")
PY

echo "qmk contract checks passed"
