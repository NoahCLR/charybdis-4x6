#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
KEYMAP_PATH="$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah"
BUILD_DIR="$(mktemp -d)"
CONFIG="$BUILD_DIR/compile_config.h"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"

{
    printf '#pragma once\n'
    printf '#include "%s/users/noah/config.h"\n' "$ROOT"
    printf '#include "%s/config.h"\n' "$KEYMAP_PATH"
} >"$CONFIG"
cp "$ROOT/tests/fixtures/client-regression/portable.bin.fixture" "$BUILD_DIR/portable.bin"
# The frozen populated profiles carry eight pointing slots (RGB v2, PD v1).
# This firmware refuses them as they are and accepts their documented
# 32-slot translation (RGB v3, sparse PD v2).
for suffix in .pd .pd3 .pd4 .pd5; do
    cp "$ROOT/tests/fixtures/client-regression/portable.bin$suffix" "$BUILD_DIR/portable.bin$suffix"
done
# Keep the integrated populated fixture on the one accepted settings version.
python3 "$ROOT/tests/host/translate_eight_slot_profile.py" "$BUILD_DIR/portable.bin.pd5" "$BUILD_DIR/portable32.bin.pd5"

build_and_run() {
    name="$1"
    shift
    bin="$BUILD_DIR/profile_compiled_defaults_v1_test_$name"
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic "$@" \
        -DNOAH_COMPILED_DEFAULTS_TEST \
        -DCOMBO_ENABLE \
        -DPOINTING_DEVICE_ENABLE \
        -DRGB_MATRIX_ENABLE \
        -DRGB_MATRIX_WS2812 \
        -DVIA_ENABLE \
        -DMCU_RP \
        -DTOTAL_EEPROM_BYTE_COUNT=0x4800u \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DQMK_KEYBOARD_H='"noah_real_profile_keyboard.h"' \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$CONFIG" \
        "$ROOT/tests/host/profile_compiled_defaults_v1_test.c" \
        "$KEYMAP_PATH/keymap.c" \
        "$KEYMAP_PATH/pd_config.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_pd_v1.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_pd_runtime.c" \
        "$KEYMAP_PATH/rgb_config.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_compiled_defaults_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
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
        -o "$bin"
    if [ "$name" = configured ] || [ "$name" = configured_sanitized ]; then
        "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" ${NOAH_WRITE_PD_FIXTURE:+--write-fixture}
        for suffix in .pd .pd3 .pd4 .pd5; do
            "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" --reject-profile "$BUILD_DIR/portable.bin$suffix"
        done
        "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" --import-profile "$BUILD_DIR/portable32.bin.pd5"
        if [ -n "${NOAH_TEST_PD_IMPORT:-}" ]; then
            "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" --import-profile "$NOAH_TEST_PD_IMPORT"
        fi
    elif [ "$name" = empty ]; then
        "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" --empty-profile "$BUILD_DIR/portable32.bin.pd5"
    fi
}

build_and_run configured -DNOAH_PD_PROFILE_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE
build_and_run configured_sanitized -DNOAH_PD_PROFILE_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE -fsanitize=address,undefined -fno-omit-frame-pointer
build_and_run empty -DNOAH_PD_PROFILE_ENABLE -DNOAH_KEYMAP_EMPTY_KEY_BEHAVIORS -DNOAH_KEYMAP_EMPTY_COMBOS -DNOAH_PORTABLE_PROFILE_ENABLE

# Keep the materializer available when RGB is compiled out: that variant emits
# the canonical key-behavior domain alone and must remain warning-clean.
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DNOAH_PD_PROFILE_ENABLE \
    -DVIA_ENABLE \
    -DMCU_RP \
    -DTOTAL_EEPROM_BYTE_COUNT=0x4800u \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DQMK_KEYBOARD_H='"noah_real_profile_keyboard.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$CONFIG" \
    -fsyntax-only \
    "$ROOT/users/noah/lib/profile/schema/profile_compiled_defaults_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_settings_defaults.c" \
    "$ROOT/users/noah/lib/profile/runtime/profile_action_runtime_v1.c"

# The golden compiled PD domain is the shared "presets" vector, and its RGB
# domain is version 3 with a row for each of the 32 slots.
python3 - "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" "$ROOT/tests/fixtures/pd_mode_domain_v2.json" <<'PY'
import json, struct, sys
from pathlib import Path
fields = dict(line.split("=", 1) for line in Path(sys.argv[1]).read_text().splitlines() if "=" in line)
blob = bytes.fromhex(fields["profile.full.hex"])
offset, domains = 8, {}
for _ in range(blob[6]):
    length = struct.unpack_from("<H", blob, offset + 2)[0]
    domains[blob[offset]] = (blob[offset + 1], blob[offset + 4 : offset + 4 + length])
    offset += 4 + length
presets = next(case for case in json.loads(Path(sys.argv[2]).read_text())["valid"] if case["name"] == "presets")
assert domains[0x50] == (2, bytes.fromhex(presets["hex"])), "compiled PD domain differs from the presets vector"
assert domains[0x10][0] == 3 and domains[0x10][1][0] == 3 and domains[0x10][1][7] == 32
PY
