#!/bin/sh

# State this firmware's contract as JSON on stdout: the Profile Wire capability
# pages the keyboard answers (tests/host/contract_probe.c), the BK commit it is
# built with (qmk-pin.json) and the SHA-256 of every fixture under
# tests/fixtures. Firmware only states what it is; the agreement check that
# judges it lives with the client. Built with the production feature set
# (users/noah/rules.mk defaults: live-profile owner and mutation, PD profile,
# split, VIA, combos, RGB matrix, pointing device).
#
#   sh tests/host/run_contract_probe.sh [--output FILE]

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
KEYMAP_PATH="$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah"
OUTPUT=""
if [ "${1:-}" = "--output" ] && [ -n "${2:-}" ]; then
    OUTPUT="$2"
elif [ $# -gt 0 ]; then
    echo "Usage: sh tests/host/run_contract_probe.sh [--output FILE]" >&2
    exit 1
fi
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"

CONFIG="$BUILD_DIR/compile_config.h"
{
    printf '#pragma once\n'
    printf '#include "%s/users/noah/config.h"\n' "$ROOT"
    printf '#include "%s/config.h"\n' "$KEYMAP_PATH"
} >"$CONFIG"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DCOMBO_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DVIA_ENABLE \
    -DSPLIT_KEYBOARD \
    -DMCU_RP \
    -DTOTAL_EEPROM_BYTE_COUNT=0x4800u \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DNOAH_LIVE_PROFILE_OWNER_ENABLE \
    -DNOAH_LIVE_PROFILE_MUTATION_ENABLE \
    -DNOAH_PORTABLE_PROFILE_ENABLE \
    -DNOAH_PD_PROFILE_ENABLE \
    -DQMK_KEYBOARD_H='"noah_real_profile_keyboard.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$CONFIG" \
    "$ROOT/tests/host/contract_probe.c" \
    "$KEYMAP_PATH/keymap.c" \
    "$KEYMAP_PATH/pd_config.c" \
    "$KEYMAP_PATH/rgb_config.c" \
    "$ROOT/users/noah/lib/profile/protocol/profile_wire_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_pd_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/effective_pd_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_compiled_defaults_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/key_behavior_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_combo_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_pd_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_settings_defaults.c" \
    "$ROOT/users/noah/lib/profile/runtime/profile_action_runtime_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/profile_action_placement_v1.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_domain_registry.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    "$ROOT/users/noah/lib/profile/schema/key_behavior_domain_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_validator_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_combo_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_settings_v1.c" \
    "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
    -o "$BUILD_DIR/contract_probe"

"$BUILD_DIR/contract_probe" >"$BUILD_DIR/pages.json"

python3 - "$ROOT" "$BUILD_DIR/pages.json" "${OUTPUT:-}" <<'PY'
import hashlib, json, subprocess, sys
from pathlib import Path

root, pages, output = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
report = json.loads(pages.read_text())
report["firmwareCommit"] = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                                          capture_output=True, text=True).stdout.strip() or None
report["qmkPin"] = json.loads((root / "qmk-pin.json").read_text())
report["fixtures"] = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                      for path in sorted((root / "tests/fixtures").rglob("*")) if path.is_file()}
text = json.dumps(report, indent=2) + "\n"
if output:
    Path(output).write_text(text)
else:
    sys.stdout.write(text)
PY
