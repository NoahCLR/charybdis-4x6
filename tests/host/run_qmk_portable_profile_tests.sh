#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
for variant in normal sanitized; do
    flags=""
    if [ "$variant" = sanitized ]; then flags="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    cc -std=c11 -Wall -Wextra -Werror $flags -DNOAH_PORTABLE_PROFILE_ENABLE -DNOAH_PD_PROFILE_ENABLE -DQMK_KEYBOARD_H='"portable_profile_keyboard.h"' \
        -I"$ROOT/tests/host/include/portable_profile" -I"$ROOT/tests/host/include" -I"$ROOT" -I"$ROOT/users/noah" \
        "$ROOT/tests/host/qmk_portable_profile_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_portable_profile.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_settings_runtime.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        -o "$BUILD_DIR/test"
    "$BUILD_DIR/test" "$BUILD_DIR/responses.fixture"
    # The app reads the firmware-produced pages and finds the macro's name.
    node - "$ROOT" "$BUILD_DIR/responses.fixture" <<'JS'
const assert = require("node:assert/strict"), fs = require("node:fs");
const {readSettings} = require(process.argv[2] + "/tools/charybdis-live-v2/core/protocol/portable-profile-v1");
const {decodeSettings, macroNamesOf} = require(process.argv[2] + "/tools/charybdis-live-v2/core/schema/settings-domain-v1");
const fixture = fs.readFileSync(process.argv[3]), pages = fixture.length / 32;
let id = 0;
readSettings({request: async request => {
    assert.ok(request[4] < pages);
    const response = Buffer.from(fixture.subarray(request[4] * 32, request[4] * 32 + 32));
    request.copy(response, 0, 0, 5);
    return response;
}}, {next: () => ++id}).then(bytes => {
    const names = macroNamesOf(decodeSettings(bytes));
    assert.equal(names[5], "Screenshot");
    assert.equal(names.filter(Boolean).length, 1);
    console.log("app reads the firmware's streamed settings and its macro names");
}).catch(error => {console.error(error); process.exitCode = 1;});
JS
done
