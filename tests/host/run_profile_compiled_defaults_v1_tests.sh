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
// Profiles the retired v1 app wrote (stored settings v2), frozen as hex.
const frozen = Object.fromEntries(fs.readFileSync(process.argv[2] + "/tests/fixtures/stored_profile_live_v1.fixture", "utf8")
    .split("\n").filter(line => line && !line.startsWith("#")).map(line => line.split("=")));
fs.writeFileSync(process.argv[3], Buffer.from(frozen["profile.hex"], "hex"));
fs.writeFileSync(process.argv[3] + '.pd', Buffer.from(frozen["pd_profile.hex"], "hex"));
// Settings v4 as the live app writes it, at its worst case: all 64 macro names
// at 20 characters. The frozen v1 import above stays the stored-v2
// compatibility check; .pd3 is a stored v3 domain with a UTF-8 name, which v4
// firmware still reads.
const app = process.argv[2] + "/tools/charybdis-live";
const {fingerprint, validateSnapshot} = require(app + "/core/model/portable-profile");
const {editMacro} = require(app + "/core/model/macro-editor");
const {decodeProfileBlob, encodeProfileBlob} = require(app + "/core/schema/profile-blob-v1");
const {encodeSettings} = require(app + "/core/schema/settings-domain-v1");
let named = require(app + "/tests/fixtures/pd-profile").document();
for (let slot = 0; slot < 64; slot++) named = editMacro({document: named, fingerprint: fingerprint(named)}, {keycode: `VIA_MACRO_${slot}`, name: `Macro ${slot} name`.padEnd(20, "!"), expectedFingerprint: fingerprint(named)});
fs.writeFileSync(process.argv[3] + '.pd4', validateSnapshot(named).profile);
const stored = validateSnapshot(require(app + "/tests/fixtures/pd-profile").document());
const {macros, ...rest} = stored.settings;
const v3 = encodeSettings({...rest, formatVersion: 3, macroNames: Array.from({length: 64}, (_, i) => i === 63 ? "Édition ⌘" : i === 0 ? "Sign-off" : "")});
const blob = decodeProfileBlob(stored.profile);
fs.writeFileSync(process.argv[3] + '.pd3', encodeProfileBlob({schema: blob.schema, domains: blob.domains.map(d => d.id === 0x40 ? {...d, version: 3, payload: v3} : d)}));
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
        "$ROOT/users/noah/lib/profile/runtime/profile_action_runtime_v1.c" \
        "$ROOT/users/noah/lib/profile/runtime/profile_action_placement_v1.c" \
        "$ROOT/users/noah/lib/action/action_kind.c" \
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
        "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" --import-profile "$BUILD_DIR/portable.bin.pd4"
        if [ -n "${NOAH_TEST_PD_IMPORT:-}" ]; then
            "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" --import-profile "$NOAH_TEST_PD_IMPORT"
        fi
    elif [ "$name" = empty ]; then
        "$bin" "$ROOT/tests/fixtures/compiled_profile_pd_v2.fixture" --empty-profile "$BUILD_DIR/portable.bin.pd"
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
    "$ROOT/users/noah/lib/profile/runtime/profile_action_runtime_v1.c"
