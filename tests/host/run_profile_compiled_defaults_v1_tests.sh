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

node - "$ROOT" "$BUILD_DIR/portable.bin" <<'JS'
const fs = require("node:fs");
const root = process.argv[2] + "/tools/charybdis-live";
const {document} = require(root + "/tests/fixtures/portable-profile");
const {validateSnapshot} = require(root + "/core/model/portable-profile");
fs.writeFileSync(process.argv[3], validateSnapshot(document()).profile);
fs.writeFileSync(process.argv[3] + '.pd', validateSnapshot(require(root + '/tests/fixtures/pd-profile').document()).profile);
// Settings v3 (named VIA macros) as the v2 app writes it. The v1 import
// above stays the stored-v2 compatibility check.
const v2 = process.argv[2] + "/tools/charybdis-live-v2";
const {fingerprint, validateSnapshot: validateV2} = require(v2 + "/core/model/portable-profile");
const {editMacro} = require(v2 + "/core/model/macro-editor");
let named = require(v2 + "/tests/fixtures/pd-profile").document();
for (const [keycode, name] of [["VIA_MACRO_0", "Sign-off"], ["VIA_MACRO_63", "Édition ⌘"]]) named = editMacro({document: named, fingerprint: fingerprint(named)}, {keycode, name, expectedFingerprint: fingerprint(named)});
fs.writeFileSync(process.argv[3] + '.pd3', validateV2(named).profile);
JS

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
        -DTOTAL_EEPROM_BYTE_COUNT=0x4000u \
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
        "$ROOT/users/noah/lib/profile/runtime/profile_action_runtime_v1.c" \
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
        "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" --import-profile "$BUILD_DIR/portable.bin.pd"
        "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" --import-profile "$BUILD_DIR/portable.bin.pd3"
        if [ -n "${NOAH_TEST_PD_IMPORT:-}" ]; then
            "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" --import-profile "$NOAH_TEST_PD_IMPORT"
        fi
    elif [ "$name" = bridge ]; then
        "$bin" "$ROOT/tests/fixtures/compiled_profile_v1.fixture"
    elif [ "$name" = empty ]; then
        "$bin" "$ROOT/tests/fixtures/compiled_profile_eight_v1.fixture" --empty-profile "$BUILD_DIR/portable.bin"
    else
        "$bin" "$ROOT/tests/fixtures/compiled_profile_eight_v1.fixture" ${NOAH_WRITE_COMPILED_FIXTURE:+--write-fixture}
    fi
}

build_and_run configured -DNOAH_PD_PROFILE_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE
build_and_run configured_sanitized -DNOAH_PD_PROFILE_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE -fsanitize=address,undefined -fno-omit-frame-pointer
build_and_run normal
build_and_run bridge -DNOAH_LEGACY_SNAPSHOT_BRIDGE
build_and_run empty -DNOAH_KEYMAP_EMPTY_KEY_BEHAVIORS -DNOAH_KEYMAP_EMPTY_COMBOS -DNOAH_PORTABLE_PROFILE_ENABLE
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

# Keep the materializer available when RGB is compiled out: that variant emits
# the canonical key-behavior domain alone and must remain warning-clean.
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DVIA_ENABLE \
    -DMCU_RP \
    -DTOTAL_EEPROM_BYTE_COUNT=0x4000u \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DQMK_KEYBOARD_H='"noah_real_profile_keyboard.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$CONFIG" \
    -fsyntax-only \
    "$ROOT/users/noah/lib/profile/schema/profile_compiled_defaults_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/profile_action_runtime_v1.c"
